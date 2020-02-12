//
// This is a basic tutorial for rendering a very simple geometric
// mesh with materials using OSPRay. All code is written using
// OSPRay's C interface from version 2.0.
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

//
// Generate frames for a movie.
//
void makeMovieFrames(OSPWorld world,
                     ospcommon::math::vec3f camPos,
                     ospcommon::math::vec3f camView, 
                     ospcommon::math::vec3f objCent, 
                     ospcommon::math::vec2i imgSize, 
                     OSPRenderer renderer,
                     OSPCamera camera,
                     float stepSize = 1.0)
{
    // Create and setup framebuffer
    OSPFrameBuffer framebuffer = ospNewFrameBuffer(imgSize[0], imgSize[1], OSP_FB_SRGBA,
        OSP_FB_COLOR | /*OSP_FB_DEPTH |*/ OSP_FB_ACCUM);
    ospResetAccumulation(framebuffer);

    float zposLow  = camPos.z;
    float zposHigh = -1.0 * zposLow;
    float zposCur  = zposLow;
    float zInc     = stepSize;
    int fIdx       = 0;
    int rSqr       = zposHigh * zposHigh;

    while (zposCur < zposHigh)
    {
        char fName[128];
        sprintf(fName, "frames/frame_%i.ppm", fIdx++);

        ospRenderFrameBlocking(framebuffer, renderer, camera, world);

        uint32_t* fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
        writePPM(fName, imgSize, fb);
        ospUnmapFrameBuffer(fb, framebuffer);
        ospResetAccumulation(framebuffer);

        zposCur += zInc;
        printf("\nX: %f, Z: %f", camPos.x, camPos.z);

        camPos.z = zposCur;
        float xsqr = rSqr - (zposCur * zposCur);

        if (xsqr <= 0.0)
            camPos.x = 0.0;
        else
            camPos.x = sqrt(xsqr);

        camView.x = objCent.x - camPos.x;
        camView.y = objCent.y - camPos.y;
        camView.z = objCent.z - camPos.z;

        ospSetParam(camera, "position", OSP_VEC3F, camPos);
        ospSetParam(camera, "direction", OSP_VEC3F, camView);
        ospCommit(camera);
    }

    while (zposCur > zposLow)
    {
        char fName[128];
        sprintf(fName, "frames/frame_%i.ppm", fIdx++);

        ospRenderFrameBlocking(framebuffer, renderer, camera, world);

        uint32_t* fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
        writePPM(fName, imgSize, fb);
        ospUnmapFrameBuffer(fb, framebuffer);
        ospResetAccumulation(framebuffer);

        zposCur -= zInc;
        printf("\nX: %f, Z: %f", camPos.x, camPos.z);

        camPos.z = zposCur;
        float xsqr = rSqr - (zposCur * zposCur);

        if (xsqr <= 0.0)
            camPos.x = 0.0;
        else
            camPos.x = -sqrt(xsqr);

        camView.x = objCent.x - camPos.x;
        camView.y = objCent.y - camPos.y;
        camView.z = objCent.z - camPos.z;

        ospSetParam(camera, "position", OSP_VEC3F, camPos);
        ospSetParam(camera, "direction", OSP_VEC3F, camView);
        ospCommit(camera);
    }

    ospRelease(framebuffer);
}


int main(int argc, const char **argv) {

    OSPError initError = ospInit(&argc, argv);

    if (initError != OSP_NO_ERROR)
        return initError;

    ospDeviceSetErrorFunc(
        ospGetCurrentDevice(), [](OSPError error, const char *errorDetails) {
            std::cerr << "OSPRay error: " << errorDetails << std::endl;
            exit(error);
        });


    // Image info
    ospcommon::math::vec2i imgSize;
    imgSize.x = 1024; // width
    imgSize.y = 768;  // height

    ospcommon::math::vec3f objCent {0.0, 0.0, 0.0};

    // Camera
    ospcommon::math::vec3f camPos  { 0.0f, 0.0f, -5.0f};
    ospcommon::math::vec3f camUp   { 0.f, 1.f, 0.f};
    ospcommon::math::vec3f camView { objCent.x - camPos.x,
                                      objCent.y - camPos.y,
                                      objCent.z - camPos.z };

    // Mesh data.
    int numVertex = 8;
    float vertex[] = { 
                       -0.5f, -0.5f,  0.5f, 
                        0.5f, -0.5f,  0.5f, 
                        0.5f,  0.5f,  0.5f,
                       -0.5f,  0.5f,  0.5f,
                       -0.5f,  0.5f, -0.5f, 
                        0.5f,  0.5f, -0.5f, 
                        0.5f, -0.5f, -0.5f, 
                       -0.5f, -0.5f, -0.5f,
                     };
    float color[] =  { 
                       1.0f, 0.0f, 0.0f, 1.0f,
                       0.0f, 1.0f, 0.0f, 1.0f,
                       0.0f, 0.0f, 1.0f, 1.0f,
                       1.0f, 0.0f, 0.0f, 1.0f,
                       1.0f, 0.0f, 0.0f, 1.0f,
                       0.0f, 1.0f, 0.0f, 1.0f,
                       0.0f, 0.0f, 1.0f, 1.0f,
                       1.0f, 0.0f, 0.0f, 1.0f
                     };
    int numIdx = 8;
    int32_t index[] = { 
                        0, 1, 2,
                        0, 3, 2,
                        0, 7, 4,
                        0, 3, 4,
                        7, 6, 5,
                        7, 4, 5,
                        1, 6, 5,
                        1, 2, 5,
                      };


    // Create and setup camera.
    OSPCamera camera = ospNewCamera("perspective");
    ospSetFloat(camera, "aspect", ((float) imgSize.x) / ((float) imgSize.y));
    ospSetParam(camera, "position", OSP_VEC3F, camPos);
    ospSetParam(camera, "direction", OSP_VEC3F, camView);
    ospSetParam(camera, "up", OSP_VEC3F, camUp);
    ospCommit(camera);

    // Create our mesh structure.
    OSPGeometry mesh = ospNewGeometry("mesh");

    OSPData vertexData = ospNewSharedData1D(vertex, OSP_VEC3F, 8);
    ospCommit(vertexData);
    ospSetObject(mesh, "vertex.position", vertexData);
    ospRelease(vertexData);

    OSPData colorData = ospNewSharedData1D(color, OSP_VEC4F, 8);
    ospCommit(colorData);
    ospSetObject(mesh, "vertex.color", colorData);
    ospRelease(colorData);

    OSPData indexData = ospNewSharedData1D(index, OSP_VEC3UI, 8);
    ospCommit(indexData);
    ospSetObject(mesh, "index", indexData);
    ospRelease(indexData);

    // Create and assign a material to the geometry.
    OSPMaterial mat = ospNewMaterial("pathtracer", "thinGlass");
    ospSetFloat(mat, "thickness", .2f);
    ospSetFloat(mat, "attenuationDistance", .2f);
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
    ospCommit(ambientLight);

    // Add light to our world.
    ospSetObjectAsData(world, "light", OSP_LIGHT, ambientLight);
    ospRelease(ambientLight);
    ospCommit(world);

    // Create renderer
    OSPRenderer renderer = ospNewRenderer("pathtracer");

    ospcommon::math::vec4f bgColor { 1.0f, 0.0f, 0.0f, 1.0f };

    ospSetInt(renderer, "pixelSamples", 10);
    //FIXME: background color not working...
    ospSetParam(renderer, "backgroundColor", OSP_VEC4F, bgColor);
    ospCommit(renderer);

    // Big budget movie.
    makeMovieFrames(world,
                    camPos, 
                    camView, 
                    objCent, 
                    imgSize, 
                    renderer,
                    camera,
                    .8);

    // Final cleanups
    ospRelease(renderer);
    ospRelease(camera);
    ospRelease(world);

    ospShutdown();

    return 0;
}
