#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {
saw::error_or<void> run_and_record(int argc, char** argv){
	using RarGit = Struct<
		Member<String, "commit">,
		Member<String, "origin">
	>;
	using RarCommand = Struct<
		Member<String, "command">,
		Member<Array<String>, "arguments">
	>;
	using Rar = Struct<
		Member<Array<RarCommand>, "commands">,
		Member<UInt64, "runtime_ns">,
		Member<RarGit, "git">
	>;

	return saw::make_void();
}
}
}
