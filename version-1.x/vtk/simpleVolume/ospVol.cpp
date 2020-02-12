//
// This is a simple example of renering a volume in vtk using
// OSPRay as the backend.
//
// Aside from the OSPRay sections, this code comes directly from
// VTK examples: https://lorensen.github.io/VTKExamples/site/Cxx/
//
// Author: Alister Maguire
// Date: Wed Feb 12 10:32:32 MST 2020
//

#include <vtkSmartPointer.h>
#include <vtkSphere.h>
#include <vtkSampleFunction.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkVolumeProperty.h>
#include <vtkCamera.h>
#include <vtkImageShiftScale.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkXMLImageDataReader.h>
#include <vtkOSPRayVolumeMapperNode.h>
#include <vtkOSPRayPass.h>
#include <vtkViewNodeFactory.h>
#include <vtkViewNode.h>

vtkViewNode *
getVolumeMapperNode(void)
{
  vtkOSPRayVolumeMapperNode *ospMapperNode = vtkOSPRayVolumeMapperNode::New();
  return ospMapperNode;
}

static void CreateImageData(vtkImageData* im);

int main(int argc, char *argv[])
{
  vtkSmartPointer<vtkImageData> imageData =
    vtkSmartPointer<vtkImageData>::New();
  if (argc < 2)
  {
    CreateImageData(imageData);
  }
  else
  {
    vtkSmartPointer<vtkXMLImageDataReader> reader =
      vtkSmartPointer<vtkXMLImageDataReader>::New();
    reader->SetFileName(argv[1]);
    reader->Update();
    imageData->ShallowCopy(reader->GetOutput());
  }

  vtkSmartPointer<vtkRenderWindow> renWin =
    vtkSmartPointer<vtkRenderWindow>::New();
  vtkSmartPointer<vtkRenderer> ren1 =
    vtkSmartPointer<vtkRenderer>::New();
  ren1->SetBackground(0.0,0.0,0.0);

  // Tell vtk to use the OSPRay rendering in place of vtkSmartVolumeMapper.
  vtkNew<vtkOSPRayPass> osprayPass;
  vtkViewNodeFactory *factory = osprayPass->GetViewNodeFactory();
  factory->RegisterOverride("vtkSmartVolumeMapper",
      getVolumeMapperNode);
  ren1->SetPass(osprayPass);

  renWin->AddRenderer(ren1);

  renWin->SetSize(301,300); // intentional odd and NPOT  width/height

  vtkSmartPointer<vtkRenderWindowInteractor> iren =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  iren->SetRenderWindow(renWin);

  renWin->Render(); // make sure we have an OpenGL context.

  vtkSmartPointer<vtkSmartVolumeMapper> volumeMapper =
    vtkSmartPointer<vtkSmartVolumeMapper>::New();
  volumeMapper->SetBlendModeToComposite(); // composite first
  volumeMapper->SetInputData(imageData);

  vtkSmartPointer<vtkVolumeProperty> volumeProperty =
    vtkSmartPointer<vtkVolumeProperty>::New();
  volumeProperty->ShadeOff();
  volumeProperty->SetInterpolationType(VTK_LINEAR_INTERPOLATION);

  vtkSmartPointer<vtkPiecewiseFunction> compositeOpacity =
    vtkSmartPointer<vtkPiecewiseFunction>::New();
  compositeOpacity->AddPoint(0.0,0.0);
  compositeOpacity->AddPoint(80.0,1.0);
  compositeOpacity->AddPoint(80.1,0.0);
  compositeOpacity->AddPoint(255.0,0.0);
  volumeProperty->SetScalarOpacity(compositeOpacity); // composite first.

  vtkSmartPointer<vtkColorTransferFunction> color =
    vtkSmartPointer<vtkColorTransferFunction>::New();
  color->AddRGBPoint(0.0  ,0.0,0.0,1.0);
  color->AddRGBPoint(40.0  ,1.0,0.0,0.0);
  color->AddRGBPoint(255.0,1.0,1.0,1.0);
  volumeProperty->SetColor(color);

  vtkSmartPointer<vtkVolume> volume =
    vtkSmartPointer<vtkVolume>::New();
  volume->SetMapper(volumeMapper);
  volume->SetProperty(volumeProperty);
  ren1->AddViewProp(volume);
  ren1->ResetCamera();

  // Render composite. In default mode. For coverage.
  renWin->Render();

  // 3D texture mode. For coverage.
#if !defined(VTK_LEGACY_REMOVE) && !defined(VTK_OPENGL2)
  volumeMapper->SetRequestedRenderModeToRayCastAndTexture();
#endif // VTK_LEGACY_REMOVE
  renWin->Render();

  // Software mode, for coverage. It also makes sure we will get the same
  // regression image on all platforms.
  volumeMapper->SetRequestedRenderModeToRayCast();
  renWin->Render();

  iren->Start();

  return EXIT_SUCCESS;
}

void CreateImageData(vtkImageData* imageData)
{
  // Create a spherical implicit function.
  vtkSmartPointer<vtkSphere> sphere =
    vtkSmartPointer<vtkSphere>::New();
  sphere->SetRadius(0.1);
  sphere->SetCenter(0.0,0.0,0.0);

  vtkSmartPointer<vtkSampleFunction> sampleFunction =
    vtkSmartPointer<vtkSampleFunction>::New();
  sampleFunction->SetImplicitFunction(sphere);
  sampleFunction->SetOutputScalarTypeToDouble();
  sampleFunction->SetSampleDimensions(127,127,127); // intentional NPOT dimensions.
  sampleFunction->SetModelBounds(-1.0,1.0,-1.0,1.0,-1.0,1.0);
  sampleFunction->SetCapping(false);
  sampleFunction->SetComputeNormals(false);
  sampleFunction->SetScalarArrayName("values");
  sampleFunction->Update();

  vtkDataArray* a = sampleFunction->GetOutput()->GetPointData()->GetScalars("values");
  double range[2];
  a->GetRange(range);

  vtkSmartPointer<vtkImageShiftScale> t =
    vtkSmartPointer<vtkImageShiftScale>::New();
  t->SetInputConnection(sampleFunction->GetOutputPort());

  t->SetShift(-range[0]);
  double magnitude=range[1]-range[0];
  if(magnitude==0.0)
  {
    magnitude=1.0;
  }
  t->SetScale(255.0/magnitude);
  t->SetOutputScalarTypeToUnsignedChar();

  t->Update();

  imageData->ShallowCopy(t->GetOutput());
}
