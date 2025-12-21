#include <vtkCamera.h>
#include <vtkSmartPointer.h>
#include <vtkStructuredPointsReader.h>  // Use vtkXMLStructuredGridReader for .vts
#include <vtkStructuredPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkActor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <sstream>
#include <iomanip>

int main()
{
    // Renderer & offscreen window
    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.1, 0.2, 0.4);

    auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(800, 600);
    renderWindow->OffScreenRenderingOn();  // No GUI

    auto windowToImage = vtkSmartPointer<vtkWindowToImageFilter>::New();
    windowToImage->SetInput(renderWindow);
    windowToImage->SetInputBufferTypeToRGB();
    windowToImage->ReadFrontBufferOff();

    auto writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetInputConnection(windowToImage->GetOutputPort());

    // Camera setup (fixed)
    renderer->GetActiveCamera()->SetPosition(0, 0, 10);
    renderer->GetActiveCamera()->SetFocalPoint(0, 0, 0);
    renderer->ResetCamera();

    int startIndex = 0;
    int endIndex = 1024; // adjust to your sequence length

    for (int i = startIndex; i <= endIndex; ++i)
    {
        // Format input filename
        std::ostringstream vtkFile;
        vtkFile << "poiseulle_2d_gpu_" << (i*4) << ".vtk";

        // Read structured points (equidistant grid)
        auto reader = vtkSmartPointer<vtkStructuredPointsReader>::New();
        reader->SetFileName(vtkFile.str().c_str());
        reader->Update();

        // Convert structured grid to surface for rendering
        auto surfaceFilter = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
        surfaceFilter->SetInputConnection(reader->GetOutputPort());
        surfaceFilter->Update();

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(surfaceFilter->GetOutputPort());

        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);

        // Clear previous frame
        renderer->RemoveAllViewProps();
        renderer->AddActor(actor);

        // Render offscreen
        renderWindow->Render();
        windowToImage->Modified();

        // Save PNG
        std::ostringstream pngFile;
        pngFile << "tmp/png/frame_" << std::setw(8) << std::setfill('0') << i << ".png";
        writer->SetFileName(pngFile.str().c_str());
        writer->Write();
    }

    return 0;
}
