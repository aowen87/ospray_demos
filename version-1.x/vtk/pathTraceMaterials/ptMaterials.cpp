//
// This is a simple example of rendering materials using
// the OSPRay backend of VTK.
//
// Author: Alister Maguire
// Date: Wed Feb 19 10:51:17 MST 2020
//

#include <vtkSmartPointer.h>

#include <vtkUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkUnstructuredGrid.h>
#include <vtkGeometryFilter.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>

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

void
readUnstructuredGrid(const char *fName, vtkUnstructuredGrid *usGrid)
{
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
}

int main(int argc, char *argv[])
{

  std::vector<std::string> materials = {"Glass", "Metal", "MetallicPaint"};

  if (argc < 3)
  {
    fprintf(stderr, "\nUsage: ./ptMaterials MaterialType VTKFile\n");
    fprintf(stderr, "\nAvailable materials: ");
    for (std::vector<std::string>::iterator it = materials.begin();
         it != materials.end(); ++it)
    {
      fprintf(stdout, "\n%s", it->c_str());
    }
    fprintf(stdout, "\n");

    return EXIT_FAILURE;
  }

  std::string chosenMat = std::string(argv[1]);
  const char *dataPath  = argv[2];

  bool validMat = false;
  for (std::vector<std::string>::iterator it = materials.begin();
       it != materials.end(); ++it)
  {
    if (chosenMat == (*it))
    {
      validMat = true;
      break;
    }
  }

  if (!validMat)
  {
    fprintf(stderr, "\nInvalid material: %s\n", chosenMat.c_str());
    fprintf(stderr, "\nAvailable materials: ");
    for (std::vector<std::string>::iterator it = materials.begin();
         it != materials.end(); ++it)
    {
      fprintf(stdout, "\n%s", it->c_str());
    }
    fprintf(stdout, "\n");
    return EXIT_FAILURE;
  }

  vtkSmartPointer<vtkNamedColors> colors = 
    vtkSmartPointer<vtkNamedColors>::New();

  vtkSmartPointer<vtkRenderer> renderer = 
    vtkSmartPointer<vtkRenderer>::New();
  renderer->UseHiddenLineRemovalOn();

  vtkSmartPointer<vtkRenderWindow> renderWindow = 
    vtkSmartPointer<vtkRenderWindow>::New();

  renderWindow->SetSize(640, 480);
  renderWindow->AddRenderer(renderer);

  auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
  interactor->SetRenderWindow(renderWindow);

  // Read in our unstructured grid.
  std::cout << "Loading: " << dataPath << std::endl;
  vtkSmartPointer<vtkUnstructuredGrid> unstructuredGrid = 
    vtkSmartPointer<vtkUnstructuredGrid>::New();
  readUnstructuredGrid(dataPath, unstructuredGrid);

  // Convert our grid to polydata.
  vtkSmartPointer<vtkGeometryFilter> geometryFilter = 
  vtkSmartPointer<vtkGeometryFilter>::New();
  geometryFilter->SetInputData(unstructuredGrid);
  geometryFilter->Update(); 

  vtkSmartPointer<vtkPolyDataNormals> normalGenerator = 
    vtkSmartPointer<vtkPolyDataNormals>::New();
  normalGenerator->SetInputData(geometryFilter->GetOutput());
  normalGenerator->ComputePointNormalsOn();
  normalGenerator->ComputeCellNormalsOn();
  normalGenerator->Update();
  
  vtkPolyData* polyData = normalGenerator->GetOutput();

  // Create a polydata mapper.
  auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInputData(polyData);
  mapper->ScalarVisibilityOff();
  mapper->Update();

  // Tell vtk to use OSPRay as a backend in place of vtkPolyDataMapper.
  vtkNew<vtkOSPRayPass> osprayPass;
  vtkViewNodeFactory *factory = osprayPass->GetViewNodeFactory();
  factory->RegisterOverride("vtkPolyDataMapper",
      getPolyDataMapperNode);
  vtkOSPRayRendererNode::SetRendererType("pathtracer", renderer);

  // Now, let's create our material.
  vtkSmartPointer<vtkOSPRayMaterialLibrary> matLib =
    vtkSmartPointer<vtkOSPRayMaterialLibrary>::New();

  if (chosenMat == "Glass")
  {
    matLib->AddMaterial("mat 1", "Glass");
    double color[]      = {1.0, 0.0, 0.0};
    double attenColor[] = {1.0, 0.0, 0.0};
    double attenDist    = 1.0;
    double thickness    = 0.2;
    matLib->AddShaderVariable("mat 1", "color", 3, color);
    matLib->AddShaderVariable("mat 1", "attenuationColor", 3, attenColor);
    matLib->AddShaderVariable("mat 1", "attenuationDistance", 1, &attenDist);
    matLib->AddShaderVariable("mat 1", "thickness", 1, &thickness);
    renderer->SetBackground(colors->GetColor3d("Honeydew").GetData());
  }
  else if (chosenMat == "Metal")
  {
    matLib->AddMaterial("mat 1", "Metal");
    double eta[3]    = {1.5, 0.98, 0.6};
    double k[3]      = {7.6, 6.6, 5.4};
    double roughness = .1;
    matLib->AddShaderVariable("mat 1", "eta", 3, eta);
    matLib->AddShaderVariable("mat 1", "k", 3, k);
    matLib->AddShaderVariable("mat 1", "roughness", 1, &roughness);
    renderer->SetBackground(colors->GetColor3d("Snow").GetData());
  }
  else if (chosenMat == "MetallicPaint")
  {
    matLib->AddMaterial("mat 1", "MetallicPaint");
    double baseColor[3] = {0.0, 0.1, 1.0};
    double flakeColor[3] = {1.0, 1.0, 1.0};
    double fSpread = .3;
    matLib->AddShaderVariable("mat 1", "baseColor", 3, baseColor);
    matLib->AddShaderVariable("mat 1", "flakeColor", 3, flakeColor);
    matLib->AddShaderVariable("mat 1", "flakeSpread", 1, &fSpread);
    renderer->SetBackground(colors->GetColor3d("Silver").GetData());
  }

  vtkOSPRayRendererNode::SetMaterialLibrary(matLib, renderer);
  vtkOSPRayRendererNode::SetSamplesPerPixel(8, renderer);

  // Specify our material in the actor.
  auto actor = vtkSmartPointer<vtkActor>::New();
  actor->SetMapper(mapper);
  actor->GetProperty()->SetMaterialName("mat 1");
  actor->GetProperty()->SetSpecular(.3);
  actor->GetProperty()->SetSpecularPower(30);
  actor->GetProperty()->EdgeVisibilityOff();

  renderer->AddActor(actor);
  renderer->GetActiveCamera()->Azimuth(45);
  renderer->GetActiveCamera()->Elevation(45);
  renderer->SetUseShadows(true);
  renderer->ResetCamera();
  renderer->SetPass(osprayPass);
  renderWindow->Render();
  interactor->Start();

  return EXIT_SUCCESS;
}
