//
// This is a basic tutorial for rendering a very simple unstructured
// volume using OSPRay. All code is written using OSPRay's C interface.
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
    if(file == nullptr) 
    {
        fprintf(stderr, "fopen('%s', 'wb') failed: %d", fileName, errno);
        return;
    }
    fprintf(file, "P6\n%i %i\n255\n", size.x, size.y);
    unsigned char *out = (unsigned char *)alloca(3*size.x);
    for (int y = 0; y < size.y; y++) 
    {
        const unsigned char *in = (const unsigned char *)&pixel[(size.y-1-y)*size.x];
        for (int x = 0; x < size.x; x++) 
        {
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


int main(int argc, const char **argv)
{
    OSPError initError = ospInit(&argc, argv);
    if (initError != OSP_NO_ERROR)
        return initError;

    ospDeviceSetErrorFunc(
        ospGetCurrentDevice(), [](OSPError error, const char *errorDetails) {
            std::cerr << "OSPRay error: " << errorDetails << std::endl;
            exit(error);
        });

    // Image size
    const ospcommon::math::vec2i imgSize {1024, 780};

    // Set up the camera
    ospcommon::math::vec3f objCent = { 0.0, 0.0, 0.0 };
    ospcommon::math::vec3f camPos    { 0.0f, 0.0f, -5.f };
    ospcommon::math::vec3f camUp     { 0.f, 1.f, 0.f };
    ospcommon::math::vec3f camView   { objCent.x - camPos.x,
                                       objCent.y - camPos.y,
                                       objCent.z - camPos.z };

    // Create and setup camera.
    OSPCamera camera = ospNewCamera("perspective");
    ospSetFloat(camera, "aspect", ((float) imgSize.x) / ((float) imgSize.y));
    ospSetParam(camera, "position", OSP_VEC3F, camPos);
    ospSetParam(camera, "direction", OSP_VEC3F, camView);
    ospSetParam(camera, "up", OSP_VEC3F, camUp);
    ospCommit(camera);

    //
    // Just imagine vertex 8 forms a pyramid with 1, 2, 5, 6....
    //
    //      7--------6 
    //     /|       /|
    //    4--------5 |
    //    | |      | |  8
    //    | 3------|-2 
    //    |/       |/ 
    //    0--------1
    //
    // I've also added a tet that sits behind this geometry.
    //
    int numCells    = 3;
    int numVertices = 13;
    float vertexPositions[] = {
        -1.0, -0.5,  0.5, // 0
         0.0, -0.5,  0.5, // 1
         0.0, -0.5, -0.5, // 2
        -1.0, -0.5, -0.5, // 3
        -1.0,  0.5,  0.5, // 4
         0.0,  0.5,  0.5, // 5
         0.0,  0.5, -0.5, // 6
        -1.0,  0.5, -0.5, // 7
         1.0,  0.0,  0.0, // 8

        -0.5, -0.5, -1.0, // 9
         0.5, -0.5, -1.0, // 10
         0.0, -0.5, -2.0, // 11
         0.0,  0.5, -1.5, // 12
    };

    int numIndices = 13;
    unsigned int indices[] = {
        0, 1, 2, 3, 4, 5, 6, 7, // Hex cell
        1, 2, 6, 5, 8,          // Pyramid cell
        9, 10, 11, 12,          // Tet cell
    };

    unsigned int cellStarts[] = {
        0, 8, 13
    };

    uint8_t cellTypes[] {
        OSP_HEXAHEDRON,
        OSP_PYRAMID,
        OSP_TETRAHEDRON,
    };

    std::vector<float> vertexData(numVertices);
    for (int i = 0; i < numVertices; ++i)
    {
        vertexData[i] = float(i);
    };

    ospcommon::math::vec2f range;
    range[0] = vertexData[0];
    range[1] = vertexData.back();

    // Create our volume.
    OSPVolume volume = ospNewVolume("unstructured");
    
    OSPData vertexPosData = ospNewSharedData1D(vertexPositions,
        OSP_VEC3F, numVertices);
    ospCommit(vertexPosData);
    ospSetParam(volume, "vertex.position", OSP_DATA, &vertexPosData);
    ospRelease(vertexPosData);

    OSPData indexData = ospNewSharedData1D(indices,
        OSP_UINT, numIndices);
    ospCommit(indexData);
    ospSetParam(volume, "index", OSP_DATA, &indexData);
    ospRelease(indexData);

    OSPData cellStartData = ospNewSharedData1D(cellStarts,
        OSP_UINT, numCells);
    ospCommit(cellStartData);
    ospSetParam(volume, "cell.index", OSP_DATA, &cellStartData);
    ospRelease(cellStartData);

    OSPData ospVertexData = ospNewSharedData1D(vertexData.data(),
        OSP_FLOAT, vertexData.size());
    ospCommit(ospVertexData);
    ospSetParam(volume, "vertex.data", OSP_DATA, &ospVertexData);
    ospRelease(ospVertexData);

    OSPData cellTypeData = ospNewSharedData1D(cellTypes,
        OSP_UINT, numCells);
    ospCommit(cellTypeData);
    ospSetParam(volume, "cell.type", OSP_DATA, &cellTypeData);
    ospRelease(cellTypeData);
    
    ospCommit(volume);

    // Set up the transfer function.
    float colors[] = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
    };
    float opacities[] = { 8.0, 1.0 };
    OSPTransferFunction tfn = ospNewTransferFunction("piecewiseLinear");

    ospSetParam(tfn, "valueRange", OSP_VEC2F, range);

    OSPData tfColorData = ospNewSharedData1D(colors, OSP_VEC3F, 3);
    ospCommit(tfColorData);
    ospSetParam(tfn, "color", OSP_DATA, &tfColorData);
    ospRelease(tfColorData);

    OSPData tfOpacityData = ospNewSharedData1D(opacities, OSP_FLOAT, 2);
    ospCommit(tfOpacityData);
    ospSetParam(tfn, "opacity", OSP_DATA, &tfOpacityData);
    ospRelease(tfOpacityData);
    ospCommit(tfn);

    // Create a model for our mesh.
    OSPVolumetricModel model = ospNewVolumetricModel(volume);
    ospSetObject(model, "transferFunction", tfn);
    ospCommit(model);
    ospRelease(tfn);

    // Create a group for our model.
    OSPGroup group = ospNewGroup();
    ospSetObjectAsData(group, "volume", OSP_VOLUMETRIC_MODEL, model);
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

    // Create OSPRay renderer
    OSPRenderer renderer = ospNewRenderer("scivis");
    ospSetFloat(renderer, "backgroundColor", 1.0f);
    ospSetInt(renderer, "aoSamples", 100);
    ospSetFloat(renderer, "aoIntensity", 10.0f);
    ospSetFloat(renderer, "volumeSamplingRate", 30.0f);
    ospCommit(renderer);

    makeMovieFrames(world,
                    camPos, 
                    camView, 
                    objCent, 
                    imgSize, 
                    renderer,
                    camera,
                    0.3);

    // Cleanup remaining objects
    ospRelease(camera);
    ospRelease(world);
    ospRelease(renderer);

    ospShutdown();

    return 0;
}
