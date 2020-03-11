//
// This is a simple example of rendering polydata in vtk using
// OSPRay as the backend.
//
// Aside from the OSPRay sections, this code comes directly from
// VTK examples: https://lorensen.github.io/VTKExamples/site/Cxx/
//
// Author: Alister Maguire
// Date: Wed Feb 12 10:32:32 MST 2020
//

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkOSPRayPolyDataMapperNode.h>
#include <vtkOSPRayPass.h>
#include <vtkViewNodeFactory.h>
#include <vtkViewNode.h>

#include <array>

vtkViewNode *
getPolyDataMapperNode(void)
{
  vtkOSPRayPolyDataMapperNode *ospMapperNode = vtkOSPRayPolyDataMapperNode::New();
  return ospMapperNode;
}

int main(int, char *[])
{
  vtkNew<vtkNamedColors> colors;

  std::array<std::array<double, 3>, 8> pts = {{{{0, 0, 0}},
                                               {{1, 0, 0}},
                                               {{1, 1, 0}},
                                               {{0, 1, 0}},
                                               {{0, 0, 1}},
                                               {{1, 0, 1}},
                                               {{1, 1, 1}},
                                               {{0, 1, 1}}}};
  // The ordering of the corner points on each face.
  std::array<std::array<vtkIdType, 4>, 6> ordering = {{{{0, 1, 2, 3}},
                                                       {{4, 5, 6, 7}},
                                                       {{0, 1, 5, 4}},
                                                       {{1, 2, 6, 5}},
                                                       {{2, 3, 7, 6}},
                                                       {{3, 0, 4, 7}}}};

  // We'll create the building blocks of polydata including data attributes.
  vtkNew<vtkPolyData> cube;
  vtkNew<vtkPoints> points;
  vtkNew<vtkCellArray> polys;
  vtkNew<vtkFloatArray> scalars;

  // Load the point, cell, and data attributes.
  for (auto i = 0ul; i < pts.size(); ++i)
  {
    points->InsertPoint(i, pts[i].data());
    scalars->InsertTuple1(i, i);
  }
  for (auto&& i : ordering)
  {
    polys->InsertNextCell(vtkIdType(i.size()), i.data());
  }

  // We now assign the pieces to the vtkPolyData.
  cube->SetPoints(points);
  cube->SetPolys(polys);
  cube->GetPointData()->SetScalars(scalars);

  // Now we'll look at it.
  vtkNew<vtkPolyDataMapper> cubeMapper;
  cubeMapper->SetInputData(cube);
  cubeMapper->SetScalarRange(cube->GetScalarRange());
  vtkNew<vtkActor> cubeActor;
  cubeActor->SetMapper(cubeMapper);

  // Tell vtk to use OSPRay as a backend in place of vtkPolyDataMapper.
  vtkNew<vtkOSPRayPass> osprayPass;

  // NOTE: accessing the view node factory and registering an override
  // is not standard. This comes from a VTK patch within VisIt. In all
  // of these examples, this section can be excluded. It is ony here as
  // guidance for VisIt developers.
  vtkViewNodeFactory *factory = osprayPass->GetViewNodeFactory();
  factory->RegisterOverride("vtkPolyDataMapper", 
      getPolyDataMapperNode);

  // The usual rendering stuff.
  vtkNew<vtkCamera> camera;
  camera->SetPosition(1, 1, 1);
  camera->SetFocalPoint(0, 0, 0);

  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkRenderWindow> renWin;
  renWin->AddRenderer(renderer);

  vtkNew<vtkRenderWindowInteractor> iren;
  iren->SetRenderWindow(renWin);

  renderer->AddActor(cubeActor);
  renderer->SetActiveCamera(camera);
  renderer->ResetCamera();
  renderer->SetBackground(colors->GetColor3d("Cornsilk").GetData());
  renderer->SetPass(osprayPass);

  renWin->SetSize(600, 600);

  // interact with data
  renWin->Render();
  iren->Start();

  return EXIT_SUCCESS;
}
