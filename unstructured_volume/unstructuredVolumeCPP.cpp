//
// This is a basic tutorial for rendering a very simple unstructured
// volume using OSPRay. All code is written using OSPRay's CPP interface.
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
void makeMovieFrames(ospray::cpp::World world,
		     ospcommon::math::vec3f camPos,
		     ospcommon::math::vec3f camView, 
		     ospcommon::math::vec3f objCent, 
		     ospcommon::math::vec2i imgSize, 
		     ospray::cpp::Renderer renderer,
		     ospray::cpp::Camera camera,
		     float stepSize = 1.0)
{

   // create and setup framebuffer
    ospray::cpp::FrameBuffer framebuffer(
	imgSize, OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM);
    framebuffer.clear();


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

	framebuffer.renderFrame(renderer, camera, world);

	uint32_t *fb = (uint32_t *)framebuffer.map(OSP_FB_COLOR);
	writePPM(fName, imgSize, fb);
	framebuffer.unmap(fb);
	framebuffer.clear();

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

	camera.setParam("position", camPos);
	camera.setParam("direction", camView);
	camera.commit();
    }

    while (zposCur > zposLow)
    {
	char fName[128];
	sprintf(fName, "frames/frame_%i.ppm", fIdx++);

	framebuffer.renderFrame(renderer, camera, world);

	uint32_t *fb = (uint32_t *)framebuffer.map(OSP_FB_COLOR);
	writePPM(fName, imgSize, fb);
	framebuffer.unmap(fb);
	framebuffer.clear();

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

	camera.setParam("position", camPos);
	camera.setParam("direction", camView);
	camera.commit();
    }
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

    {
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
        int numCells = 3;
        int numVertices = 13;
        std::vector<ospcommon::math::vec3f> vertexPositions = {
            {-1.0, -0.5,  0.5}, // 0
            { 0.0, -0.5,  0.5}, // 1
            { 0.0, -0.5, -0.5}, // 2
            {-1.0, -0.5, -0.5}, // 3
            {-1.0,  0.5,  0.5}, // 4
            { 0.0,  0.5,  0.5}, // 5
            { 0.0,  0.5, -0.5}, // 6
            {-1.0,  0.5, -0.5}, // 7
            { 1.0,  0.0,  0.0}, // 8
            {-0.5, -0.5, -1.0}, // 9
            { 0.5, -0.5, -1.0}, // 10
            { 0.0, -0.5, -2.0}, // 11
            { 0.0,  0.5, -1.5}, // 12
        };

        int numIndices = 13;
        std::vector<uint32_t> indices = {
            0, 1, 2, 3, 4, 5, 6, 7,  // Hex cell 1
            1, 2, 6, 5, 8,           // Pyramid cell
            9, 10, 11, 12,          // Tet cell
        };

        std::vector<uint32_t> cellStarts = {
            0, 8, 13
        };

        std::vector<uint8_t> cellTypes = {
            OSP_HEXAHEDRON,
            OSP_PYRAMID,
            OSP_TETRAHEDRON,
        };

        std::vector<float> cellData(numCells);
        for (int i = 0; i < numCells; ++i)
        {
            cellData[i] = float(i);
        };

        ospcommon::math::vec2f range;
        range[0] = cellData[0];
        range[1] = cellData.back();

        // Create our volume.
        ospray::cpp::Volume volume("unstructured");
        volume.setParam("vertex.position", ospray::cpp::Data(vertexPositions));
        volume.setParam("index", ospray::cpp::Data(indices));
        volume.setParam("cell.index", ospray::cpp::Data(cellStarts));
        volume.setParam("cell.data", ospray::cpp::Data(cellData));
        volume.setParam("cell.type", ospray::cpp::Data(cellTypes));
        volume.commit(); 

        // Set up the transfer function.
        std::vector<ospcommon::math::vec3f> colors = {
            {1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {0.0, 0.0, 1.0},
        };
        std::vector<float> opacities = {0.8, 1.0};

        // Set up our transfer function.
        ospray::cpp::TransferFunction transferFunction("piecewiseLinear");
        transferFunction.setParam("color", ospray::cpp::Data(colors));
        transferFunction.setParam("opacity", ospray::cpp::Data(opacities));
        transferFunction.setParam("valueRange", range);
        transferFunction.commit();

        // Set up our model.
        ospray::cpp::VolumetricModel model(volume);
        model.setParam("transferFunction", transferFunction);
        model.commit();

        // Create a group to house our model.
        ospray::cpp::Group group;
        group.setParam("volume", ospray::cpp::Data(model));
        group.commit();

        // Create our instance. 
        ospray::cpp::Instance instance(group);
        instance.commit();

        // Create our world.
        ospray::cpp::World world;
        world.setParam("instance", ospray::cpp::Data(instance));

        // Create our light.
        ospray::cpp::Light light("ambient");
        light.commit();

        world.setParam("light", ospray::cpp::Data(light));
        world.commit();

        // Create our renderer.
        ospray::cpp::Renderer renderer("scivis");
        renderer.setParam("backgroundColor", 1.0f);
        renderer.setParam("aoSamples", 100);
        renderer.setParam("aoIntensity", 10000.0f);
        renderer.setParam("volumeSamplingRate", 30.0f);
        renderer.commit();

        // Action.
        makeMovieFrames(world,
                        camPos, 
                        camView, 
                        objCent, 
                        imgSize, 
                        renderer,
                        camera,
                        0.3);
    };

    ospShutdown();

    return 0;
}
