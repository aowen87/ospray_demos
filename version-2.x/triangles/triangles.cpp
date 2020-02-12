//
// This is a basic tutorial for rendering a very simple geometric
// mesh using OSPRay version 2.0.
//
// This is largely based off of ospTutorial.c, located within the
// OSPRay repo.
//
// Author: Alister Maguire
// Date: Fri Feb  7 09:45:50 PST 2020
//

#include <memory>
#include <random>
#include <alloca.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <vector>

#include "ospray/ospray.h"
#include "ospray/ospray_cpp.h"

//
// Helper function to write the rendered image as PPM file.
// Taken from opsray examples.
//
void writePPM(const char *fileName,
              const ospcommon::math::vec2i &size,
              const uint32_t *pixel)
{
  FILE *file = fopen(fileName, "wb");
  if(file == nullptr) {
    fprintf(stderr, "fopen('%s', 'wb') failed: %d", fileName, errno);
    return;
  }
  fprintf(file, "P6\n%i %i\n255\n", size.x, size.y);
  unsigned char *out = (unsigned char *)alloca(3*size.x);
  for (int y = 0; y < size.y; y++) {
    const unsigned char *in = (const unsigned char *)&pixel[(size.y-1-y)*size.x];
    for (int x = 0; x < size.x; x++) {
      out[3*x + 0] = in[4*x + 0];
      out[3*x + 1] = in[4*x + 1];
      out[3*x + 2] = in[4*x + 2];
    }
    fwrite(out, 3*size.x, sizeof(char), file);
  }
  fprintf(file, "\n");
  fclose(file);
}


int main(int argc, const char **argv) {

    OSPError initError = ospInit(&argc, argv);

    if (initError != OSP_NO_ERROR)
        return initError;

    // Set an error callback to catch any OSPRay errors and exit the application
    ospDeviceSetErrorFunc(
        ospGetCurrentDevice(), [](OSPError error, const char *errorDetails) {
            std::cerr << "OSPRay error: " << errorDetails << std::endl;
            exit(error);
        });


    // image size
    ospcommon::math::vec2i imgSize;
    imgSize.x = 1024; // width
    imgSize.y = 768; // height

    ospcommon::math::vec3f objFace = {0.0, 0.0, 0.0};

    // camera
    ospcommon::math::vec3f camPos  { 0.0f, 0.0f, -2.0f };
    ospcommon::math::vec3f camUp   { 0.f, 1.f, 0.f };
    ospcommon::math::vec3f camView { objFace.x - camPos.x,
                                     objFace.y - camPos.y,
                                     objFace.z - camPos.z };

    // triangle mesh data
    float vertex[] = { -0.5f, -0.5f, 0.0f, 
                        0.5f, -0.5f, 0.0f, 
                        0.5f,  0.5f, 0.0f,
                       -0.5f,  0.5f, 0.0f };
    float color[]  =  { 0.9f, 0.5f, 0.5f, 1.0f,
                        0.8f, 0.8f, 0.8f, 1.0f,
                        0.8f, 0.8f, 0.8f, 1.0f,
                        0.8f, 0.8f, 0.8f, 1.0f};
    int32_t index[] = { 0, 1, 2,
                        0, 3, 2};

    // Create and setup camera
    OSPCamera camera = ospNewCamera("perspective");
    ospSetFloat(camera, "aspect", ((float) imgSize.x) / ((float) imgSize.y));
    ospSetParam(camera, "position", OSP_VEC3F, camPos);
    ospSetParam(camera, "direction", OSP_VEC3F, camView);
    ospSetParam(camera, "up", OSP_VEC3F, camUp);
    ospCommit(camera);

    // Create and setup model and mesh
    OSPGeometry mesh = ospNewGeometry("mesh");

    OSPData vertexData = ospNewSharedData1D(vertex, OSP_VEC3F, 4);
    ospCommit(vertexData);
    ospSetObject(mesh, "vertex.position", vertexData);    
    ospRelease(vertexData);

    OSPData colorData = ospNewSharedData1D(color, OSP_VEC4F, 4);
    ospCommit(colorData);
    ospSetObject(mesh, "vertex.color", colorData);
    ospRelease(colorData);

    OSPData indexData = ospNewSharedData1D(index, OSP_VEC3UI, 2);
    ospCommit(indexData);
    ospSetObject(mesh, "index", indexData);
    ospRelease(indexData);

    // Create and assign a material to the geometry
    OSPMaterial mat = ospNewMaterial("pathtracer", "obj");
    ospCommit(mat);
    ospCommit(mesh);

    // Create a model for our mesh.
    OSPGeometricModel model = ospNewGeometricModel(mesh);
    ospSetObject(model, "material", mat);
    ospCommit(model);
    ospRelease(mesh);
    ospRelease(mat);

    // Create a group for our model(s).
    OSPGroup group = ospNewGroup();
    ospSetObjectAsData(group, "geometry", OSP_GEOMETRIC_MODEL, model);
    ospCommit(group);
    ospRelease(model);

    // Create an instance of our group.
    OSPInstance instance = ospNewInstance(group);
    ospCommit(instance);
    ospRelease(group);

    // Create a world for our instance.
    OSPWorld world = ospNewWorld();
    ospSetObjectAsData(world, "instance", OSP_INSTANCE, instance);
    ospRelease(instance);

    // Create and setup light for Ambient Occlusion
    OSPLight ambientLight = ospNewLight("ambient");
    //ospSet3f(ambientLight, "color", 1.f, 1.f, 1.f);
    ospCommit(ambientLight);

    // Add light to our world.
    ospSetObjectAsData(world, "light", OSP_LIGHT, ambientLight);
    ospRelease(ambientLight);
    ospCommit(world);

    // Create renderer
    OSPRenderer renderer = ospNewRenderer("pathtracer");

    ospcommon::math::vec4f bgColor { 0.0f, 1.0f, 0.0f, 1.0f };

    ospSetInt(renderer, "pixelSamples", 5);
    //FIXME: setting background color doesn't seem to work...
    //ospSetFloat(renderer, "backgroundColor", 0.0f);
    ospSetParam(renderer, "backgroundColor", OSP_VEC4F, bgColor);
    ospCommit(renderer);

    // Create and setup framebuffer
    OSPFrameBuffer framebuffer = ospNewFrameBuffer(imgSize[0], imgSize[1], OSP_FB_SRGBA,
        OSP_FB_COLOR | /*OSP_FB_DEPTH |*/ OSP_FB_ACCUM);
    ospResetAccumulation(framebuffer);

    // Render one frame
    ospRenderFrameBlocking(framebuffer, renderer, camera, world);

    uint32_t* fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
    writePPM("firstFrame.ppm", imgSize, fb);
    ospUnmapFrameBuffer(fb, framebuffer);

    // Render 10 more frames, which are accumulated to result in a better 
    // converged image
    for (int frames = 0; frames < 10; frames++)
    {
        ospRenderFrameBlocking(framebuffer, renderer, camera, world);
    }

    fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
    writePPM("accumulatedFrames.ppm", imgSize, fb);
    ospUnmapFrameBuffer(fb, framebuffer);

    // Final cleanups
    ospRelease(renderer);
    ospRelease(camera);
    ospRelease(framebuffer);
    ospRelease(world);

    ospShutdown();

    return 0;
}
