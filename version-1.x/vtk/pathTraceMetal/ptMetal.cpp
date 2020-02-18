//
// This is a simple example of path tracing an unstructured
// grid with metal as an applied material. The frontend is
// VTK, and the backend is OSPRay.
//
// Some of this code comes from the VTK examples:
// https://lorensen.github.io/VTKExamples/site/Cxx/
//
// Author: Alister Maguire
// Date: Wed Feb 12 10:32:32 MST 2020
//

#include <vtkSmartPointer.h>

#include <vtkUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkUnstructuredGrid.h>
#include <vtkSphereSource.h>
#include <vtkAppendFilter.h>
#include <vtkGeometryFilter.h>
#include <vtkPolyData.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>

#include <vtkNamedColors.h>
#include <vtksys/SystemTools.hxx>

#include <vtkOSPRayPolyDataMapperNode.h>
#include <vtkOSPRayMaterialLibrary.h>
#include <vtkOSPRayRendererNode.h>
#include <vtkOSPRayPass.h>
#include <vtkViewNodeFactory.h>
#include <vtkViewNode.h>

#include <string>
#include <algorithm>
#include <array>

vtkViewNode *
getPolyDataMapperNode(void)
{
  vtkOSPRayPolyDataMapperNode *ospMapperNode = vtkOSPRayPolyDataMapperNode::New();
  return ospMapperNode;
}

vtkUnstructuredGrid *
readUnstructuredGrid(const char *fName)
{
  vtkUnstructuredGrid *usGrid = vtkUnstructuredGrid::New();
  std::string extension =
    vtksys::SystemTools::GetFilenameLastExtension(std::string(fName));

  std::transform(extension.begin(), extension.end(),
                 extension.begin(), ::tolower);

  if (extension == ".vtu")
  {
    auto reader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
    reader->SetFileName (fName);
    reader->Update();
    usGrid->ShallowCopy(reader->GetOutput());
  }
  else if (extension == ".vtk")
  {
    auto reader = vtkSmartPointer<vtkUnstructuredGridReader>::New();
    reader->SetFileName(fName);
    reader->Update();
    usGrid->ShallowCopy(reader->GetOutput());
  }
  else
  {
    fprintf(stderr, "\nERROR: Unknown file extension %s\n", extension.c_str());
  }

  return usGrid;
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(stderr, "\nUsage: ./usReader VTKFile\n");
    return EXIT_FAILURE;
  }

  vtkSmartPointer<vtkNamedColors> colors = 
    vtkSmartPointer<vtkNamedColors>::New();

  vtkSmartPointer<vtkRenderer> renderer = 
    vtkSmartPointer<vtkRenderer>::New();

  vtkSmartPointer<vtkRenderWindow> renderWindow = 
    vtkSmartPointer<vtkRenderWindow>::New();

  renderWindow->SetSize(640, 480);
  renderWindow->AddRenderer(renderer);

  auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
  interactor->SetRenderWindow(renderWindow);

  //renderer->SetBackground(colors->GetColor3d("Wheat").GetData());
  renderer->SetBackground(colors->GetColor3d("Silver").GetData());
  renderer->UseHiddenLineRemovalOn();

  // Read in our unstructured grid.
  std::cout << "Loading: " << argv[1] << std::endl;
  auto unstructuredGrid = readUnstructuredGrid(argv[1]);

  // Convert our grid to polydata.
  vtkSmartPointer<vtkGeometryFilter> geometryFilter = 
  vtkSmartPointer<vtkGeometryFilter>::New();
  geometryFilter->SetInputData(unstructuredGrid);
  geometryFilter->Update(); 
 
  vtkPolyData* polyData = geometryFilter->GetOutput();

  // Create a polydata mapper.
  auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInputData(polyData);
  mapper->ScalarVisibilityOn();
  mapper->Update();

  // Tell vtk to use OSPRay as a backend in place of vtkPolyDataMapper.
  vtkNew<vtkOSPRayPass> osprayPass;
  vtkViewNodeFactory *factory = osprayPass->GetViewNodeFactory();
  factory->RegisterOverride("vtkPolyDataMapper",
      getPolyDataMapperNode);
  vtkOSPRayRendererNode::SetRendererType("pathtracer", renderer);

  // Here we create out OSPRay material.
  vtkSmartPointer<vtkOSPRayMaterialLibrary> matLib =
    vtkSmartPointer<vtkOSPRayMaterialLibrary>::New();
  matLib->AddMaterial("mat 1", "MetallicPaint");
  // Base color of paint.
  double baseColor[3] = {0.0, 0.1, 1.0};
  // Reflective color.
  double flakeColor[3] = {1.0, 1.0, 1.0};
  // Reflective spread.
  double fSpread = .3;
  matLib->AddShaderVariable("mat 1", "baseColor", 3, baseColor);
  matLib->AddShaderVariable("mat 1", "flakeColor", 3, flakeColor);
  matLib->AddShaderVariable("mat 1", "flakeSpread", 1, &fSpread);
  vtkOSPRayRendererNode::SetMaterialLibrary(matLib, renderer);
  vtkOSPRayRendererNode::SetRendererType("pathtracer", renderer);
  vtkOSPRayRendererNode::SetSamplesPerPixel(8, renderer);

  // Specify our material in the actor.
  auto actor = vtkSmartPointer<vtkActor>::New();
  actor->SetMapper(mapper);
  actor->GetProperty()->SetMaterialName("mat 1");
  actor->GetProperty()->SetSpecular(.3);
  actor->GetProperty()->SetSpecularPower(30);
  actor->GetProperty()->EdgeVisibilityOn();

  renderer->AddActor(actor);
  renderer->GetActiveCamera()->Azimuth(45);
  renderer->GetActiveCamera()->Elevation(45);
  renderer->SetUseShadows(true);
  renderer->ResetCamera();
  renderer->SetPass(osprayPass);
  renderWindow->Render();
  interactor->Start();

  if (unstructuredGrid != NULL)
  {
    unstructuredGrid->Delete();
  }

  return EXIT_SUCCESS;
}
