#include "orchestrator.hpp"

#include <kel/lbm/swarm/signal.hpp>
#include <kel/lbm/swarm/wire.hpp>

#include <iostream>
#include <utility>

namespace kel {
namespace lbm {
namespace swarm {

namespace {

/**
 * libdatachannel delivers either text or binary. The control and signalling
 * protocols are both JSON text, so binary is simply not ours and is dropped.
 */
std::string text_of(const rtc::message_variant &msg){
	if(std::holds_alternative<rtc::string>(msg)){
		return std::get<rtc::string>(msg);
	}
	return {};
}

} // namespace

orchestrator::orchestrator(orchestrator_config cfg) : cfg_{std::move(cfg)} {}

orchestrator::~orchestrator(){
	stop();
}

void orchestrator::on_message(message_handler handler){
	on_message_ = std::move(handler);
}

void orchestrator::on_channel_open(
	std::function<void(const std::shared_ptr<session> &)> handler){
	on_open_ = std::move(handler);
}

void orchestrator::on_channel_closed(
	std::function<void(const std::shared_ptr<session> &)> handler){
	on_closed_ = std::move(handler);
}

std::string orchestrator::next_session_id(){
	// Called under the lock.
	return "w" + std::to_string(++next_id_);
}

uint16_t orchestrator::listen(){
	rtc::WebSocketServerConfiguration wscfg;
	wscfg.port = cfg_.port;
	wscfg.enableTls = false;

	server_ = std::make_unique<rtc::WebSocketServer>(std::move(wscfg));
	server_->onClient([this](std::shared_ptr<rtc::WebSocket> ws){
		accept_client(std::move(ws));
	});

	return server_->port();
}

void orchestrator::accept_client(std::shared_ptr<rtc::WebSocket> ws){
	auto sess = std::make_shared<session>();
	sess->ws = std::move(ws);

	{
		std::lock_guard<std::mutex> lock{mutex_};
		sess->id = next_session_id();
		sessions_.emplace(sess->id, sess);
	}

	// Callbacks capture weakly. A worker that drops mid-negotiation would
	// otherwise keep its own session alive through the capture and never be
	// collected.
	std::weak_ptr<session> weak = sess;

	sess->ws->onMessage([this, weak](rtc::message_variant msg){
		auto s = weak.lock();
		if(not s){
			return;
		}
		const std::string text = text_of(msg);
		if(text.empty()){
			return;
		}
		handle_signal(s, text);
	});

	sess->ws->onClosed([this, weak](){
		auto s = weak.lock();
		if(not s){
			return;
		}
		// The data channel may already be gone; dropping the session is
		// idempotent so the two paths cannot double-remove.
		drop_session(s->id);
	});

	begin_negotiation(sess);
}

void orchestrator::begin_negotiation(const std::shared_ptr<session> &sess){
	rtc::Configuration rtccfg;
	if(not cfg_.stun_url.empty()){
		rtccfg.iceServers.emplace_back(cfg_.stun_url);
	}

	sess->pc = std::make_shared<rtc::PeerConnection>(rtccfg);

	std::weak_ptr<session> weak = sess;

	sess->pc->onLocalDescription([weak](rtc::Description desc){
		auto s = weak.lock();
		if(not s or not s->ws or not s->ws->isOpen()){
			return;
		}

		data<sch::SignalDescription> out;
		out.get<"type">().set(desc.typeString());
		out.get<"sdp">().set(std::string{desc});

		auto eo_text = encode_message<sch::SignalDescription>(out);
		if(eo_text.is_error()){
			return;
		}
		s->ws->send(eo_text.get_value());
	});

	sess->pc->onLocalCandidate([weak](rtc::Candidate cand){
		auto s = weak.lock();
		if(not s or not s->ws or not s->ws->isOpen()){
			return;
		}

		data<sch::SignalCandidate> out;
		out.get<"type">().set(std::string{sig::candidate});
		out.get<"candidate">().set(cand.candidate());
		out.get<"mid">().set(cand.mid());

		auto eo_text = encode_message<sch::SignalCandidate>(out);
		if(eo_text.is_error()){
			return;
		}
		s->ws->send(eo_text.get_value());
	});

	sess->pc->onStateChange([this, weak](rtc::PeerConnection::State state){
		auto s = weak.lock();
		if(not s){
			return;
		}
		if(state == rtc::PeerConnection::State::Closed or
		   state == rtc::PeerConnection::State::Failed){
			drop_session(s->id);
		}
	});

	/**
	 * Creating the channel here is what triggers negotiation: with automatic
	 * negotiation on, the offer is generated because there is now something to
	 * offer. Doing it from the orchestrator rather than waiting for the worker
	 * also fixes the ordering -- the channel is in the offer, so the worker
	 * never has to guess whether it exists yet.
	 */
	sess->dc = sess->pc->createDataChannel("control");

	sess->dc->onOpen([this, weak](){
		auto s = weak.lock();
		if(not s){
			return;
		}
		if(on_open_){
			on_open_(s);
		}
	});

	sess->dc->onMessage([this, weak](rtc::message_variant msg){
		auto s = weak.lock();
		if(not s){
			return;
		}
		const std::string text = text_of(msg);
		if(text.empty()){
			return;
		}
		if(on_message_){
			on_message_(s, text);
		}
	});

	sess->dc->onClosed([this, weak](){
		auto s = weak.lock();
		if(not s){
			return;
		}
		drop_session(s->id);
	});
}

void orchestrator::handle_signal(const std::shared_ptr<session> &sess,
								 const std::string &text){
	// Two stage decode, same as the control protocol: learn the type without
	// committing to a payload shape.
	auto eo_hdr = decode_message<sch::SignalHeader>(text);
	if(eo_hdr.is_error()){
		std::cerr << "[" << sess->id << "] malformed signalling message\n";
		return;
	}

	const std::string type = eo_hdr.get_value().get<"type">().stl_string();

	if(type == sig::answer or type == sig::offer){
		auto eo = decode_message<sch::SignalDescription>(text);
		if(eo.is_error()){
			std::cerr << "[" << sess->id << "] bad " << type << "\n";
			return;
		}
		try {
			sess->pc->setRemoteDescription(
				rtc::Description{eo.get_value().get<"sdp">().stl_string(), type});
		} catch (const std::exception &e){
			std::cerr << "[" << sess->id << "] setRemoteDescription: "
					  << e.what() << "\n";
		}
		return;
	}

	if(type == sig::candidate){
		auto eo = decode_message<sch::SignalCandidate>(text);
		if(eo.is_error()){
			std::cerr << "[" << sess->id << "] bad candidate\n";
			return;
		}
		try {
			sess->pc->addRemoteCandidate(
				rtc::Candidate{eo.get_value().get<"candidate">().stl_string(),
							   eo.get_value().get<"mid">().stl_string()});
		} catch (const std::exception &e){
			std::cerr << "[" << sess->id << "] addRemoteCandidate: "
					  << e.what() << "\n";
		}
		return;
	}

	std::cerr << "[" << sess->id << "] unknown signalling type: " << type << "\n";
}

void orchestrator::drop_session(const std::string &id){
	std::shared_ptr<session> sess;

	{
		std::lock_guard<std::mutex> lock{mutex_};
		auto iter = sessions_.find(id);
		if(iter == sessions_.end()){
			// Already dropped. Several teardown paths converge here -- socket
			// closed, channel closed, peer connection failed -- and whichever
			// arrives first wins.
			return;
		}
		sess = iter->second;
		sessions_.erase(iter);
	}

	// Notify outside the lock: the handler may well want to send to other
	// sessions, and holding the registry lock across user code is how this
	// kind of thing deadlocks later.
	if(on_closed_){
		on_closed_(sess);
	}
}

bool orchestrator::send(const std::shared_ptr<session> &sess,
						const std::string &text){
	if(not sess or not sess->dc or not sess->dc->isOpen()){
		return false;
	}
	return sess->dc->send(text);
}

std::shared_ptr<session> orchestrator::find_session(const std::string &id) const {
	std::lock_guard<std::mutex> lock{mutex_};

	auto iter = sessions_.find(id);
	if(iter == sessions_.end()){
		return nullptr;
	}

	return iter->second;
}

std::size_t orchestrator::session_count() const {
	std::lock_guard<std::mutex> lock{mutex_};
	return sessions_.size();
}

void orchestrator::for_each_session(
	const std::function<void(const std::shared_ptr<session> &)> &fn) const {
	std::lock_guard<std::mutex> lock{mutex_};
	for(const auto &entry : sessions_){
		fn(entry.second);
	}
}

void orchestrator::stop(){
	std::map<std::string, std::shared_ptr<session>> taken;

	{
		std::lock_guard<std::mutex> lock{mutex_};
		taken.swap(sessions_);
	}

	// Close outside the lock. Closing a peer connection runs callbacks, and
	// some of those callbacks want the lock.
	for(auto &entry : taken){
		auto &sess = entry.second;
		if(sess->dc){
			sess->dc->close();
		}
		if(sess->pc){
			sess->pc->close();
		}
		if(sess->ws){
			sess->ws->close();
		}
	}

	if(server_){
		server_->stop();
		server_.reset();
	}
}

} // namespace swarm
} // namespace lbm
} // namespace kel
