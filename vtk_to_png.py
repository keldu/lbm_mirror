import vtk
import glob
import os

vtk_files = sorted(glob.glob("*.vtk"))
output_dir = "frames"
os.makedirs(output_dir, exist_ok=True)

renderer = vtk.vtkRenderer()
render_window = vtk.vtkRenderWindow()
render_window.SetOffScreenRendering(1)
render_window.SetSize(1024, 512)
render_window.AddRenderer(renderer)

renderer.SetBackground(1, 1, 1)

camera_initialized = False

for i, vtk_file in enumerate(vtk_files):
    reader = vtk.vtkStructuredPointsReader()
    reader.SetFileName(vtk_file)
    reader.Update()

    image = reader.GetOutput()

    mag = vtk.vtkImageMagnitude();
    mag.SetInputData(image);
    mag.Update();
    image = mag.GetOutput();
    scalar_name = "Velocity magnitude";

    scalars = image.GetPointData().GetScalars()
    if scalars is None:
        raise RuntimeError("No scalar data found")

    vmin, vmax = scalars.GetRange()

    # Image slice mapper (2D!)
    slice_mapper = vtk.vtkImageSliceMapper()
    slice_mapper.SetInputData(image)

    # Pick the only slice (or middle if dimension >1)
    extent = image.GetExtent()
    slice_mapper.SetSliceNumber(extent[4])  # Z slice

    slice_actor = vtk.vtkImageSlice()
    slice_actor.SetMapper(slice_mapper)

    # Grayscale mapping
    prop = slice_actor.GetProperty()
    prop.SetColorWindow(vmax - vmin)
    prop.SetColorLevel(0.5 * (vmax + vmin))

    renderer.RemoveAllViewProps()
    renderer.AddViewProp(slice_actor)

    if not camera_initialized:
        renderer.ResetCamera()
        camera_initialized = True

    render_window.Render()

    # Capture to PNG
    w2i = vtk.vtkWindowToImageFilter()
    w2i.SetInput(render_window)
    w2i.ReadFrontBufferOff()
    w2i.Update()

    writer = vtk.vtkPNGWriter()
    writer.SetFileName(f"{output_dir}/frame_{i:04d}.png")
    writer.SetInputConnection(w2i.GetOutputPort())
    writer.Write()

    print(f"Saved frame {i} range=({vmin},{vmax})")

