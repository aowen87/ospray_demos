#include <memory>
#include <random>
#include <alloca.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <vector>

#include "ospray_cpp.h"

//
// Helper function to write the rendered image as PPM file.
// Taken from opsray examples.
//
void writePPM(const char *fileName,
              const osp::vec2i &size,
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
// Generate frames for a short film.
//
void makeMovieFrames(osp::vec3f cam_pos,
                     osp::vec3f cam_view, 
                     osp::vec3f obj_face, 
                     osp::vec2i imgSize, 
                     OSPRenderer renderer,
                     OSPCamera camera,
                     float stepSize = 1.0)
{
    OSPFrameBuffer framebuffer = ospNewFrameBuffer(imgSize, 
        OSP_FB_SRGBA, OSP_FB_COLOR | /*OSP_FB_DEPTH |*/ OSP_FB_ACCUM);
    ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);

    float zpos_low  = cam_pos.z;
    float zpos_high = -1.0 * zpos_low;
    float zpos_cur  = zpos_low;
    float z_inc     = stepSize;
    int f_idx       = 0;
    int rsqr        = zpos_high * zpos_high;

    while (zpos_cur < zpos_high)
    {
        char f_name[128];
        sprintf(f_name, "frames/frame_%i.ppm", f_idx++);

        ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);
        for (int frames = 0; frames < 10; frames++)
          ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR | OSP_FB_ACCUM);

        uint32_t* fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
        writePPM(f_name, imgSize, fb);
        ospUnmapFrameBuffer(fb, framebuffer);

        zpos_cur += z_inc;
        printf("\nX: %f, Z: %f", cam_pos.x, cam_pos.z);

        cam_pos.z = zpos_cur;
        float xsqr = rsqr - (zpos_cur * zpos_cur);

        if (xsqr <= 0.0)
            cam_pos.x = 0.0;
        else
            cam_pos.x = sqrt(xsqr);

        cam_view.x = obj_face.x - cam_pos.x;
        cam_view.y = obj_face.y - cam_pos.y;
        cam_view.z = obj_face.z - cam_pos.z;

        ospSet3f(camera, "pos", cam_pos.x, cam_pos.y, cam_pos.z);
        ospSet3f(camera, "dir", cam_view.x, cam_view.y, cam_view.z);
        ospCommit(camera);
    }

    while (zpos_cur > zpos_low)
    {
        char f_name[128];
        sprintf(f_name, "frames/frame_%i.ppm", f_idx++);

        ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);
        for (int frames = 0; frames < 10; frames++)
          ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR | OSP_FB_ACCUM);

        uint32_t* fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
        writePPM(f_name, imgSize, fb);
        ospUnmapFrameBuffer(fb, framebuffer);

        zpos_cur -= z_inc;
        printf("\nX: %f, Z: %f", cam_pos.x, cam_pos.z);

        cam_pos.z = zpos_cur;
        float xsqr = rsqr - (zpos_cur * zpos_cur);

        if (xsqr <= 0.0)
            cam_pos.x = 0.0;
        else
            cam_pos.x = -sqrt(xsqr);

        cam_view.x = obj_face.x - cam_pos.x;
        cam_view.y = obj_face.y - cam_pos.y;
        cam_view.z = obj_face.z - cam_pos.z;

        ospSet3f(camera, "pos", cam_pos.x, cam_pos.y, cam_pos.z);
        ospSet3f(camera, "dir", cam_view.x, cam_view.y, cam_view.z);
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
    const osp::vec2i imgSize {1024, 780};

    // Create our volume.
    ospcommon::vec3l dims {5, 5, 5};
    osp::vec3f spacing {1.0, 1.0, 1.0};

    // Center the box at roughly 0, 0, 0.
    osp::vec3f origin { 
                        (float)(-1.0 * (dims.x / 2.0)), 
                        (float)(-1.0 * (dims.y / 2.0)),
                        (float)(-1.0 * (dims.z / 2.0))
                      };

    int numVoxels = dims.product();
    std::vector<float> voxels(numVoxels);
    ospcommon::vec2f range;
    range[0] = FLT_MAX;
    range[1] = -FLT_MAX;

    float scalar = 0.0;
    for (int i = 0; i < numVoxels; ++i)
    {
       voxels[i] = scalar;
       scalar += 0.987;

       if (scalar < range[0])
           range[0] = scalar; 
       if (scalar > range[1])
           range[1] = scalar; 
    }

    OSPVolume volume = ospNewVolume("shared_structured_volume");

    OSPData voxelData = ospNewData(numVoxels, OSP_FLOAT, voxels.data());
    ospSetObject(volume, "voxelData", voxelData);
    ospRelease(voxelData);

    ospSetString(volume, "voxelType", "float");
    ospSet3i(volume, "dimensions", dims.x, dims.y, dims.z);
    ospSet3f(volume, "gridSpacing", spacing.x, spacing.y, spacing.z);
    ospSet3f(volume, "gridOrigin", origin.x, origin.y, origin.z);
    ospSet2f(volume, "voxelRange", range.x, range.y);
    ospSet1f(volume, "samplingRate", 100.0);

    // Set up the transfer function.
    OSPTransferFunction tfn = ospNewTransferFunction("piecewise_linear");
    float colors[] = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
    };
    float opacities[] = { 0.0, 1.0 };

    ospSet2f(tfn, "valueRange", range[0], range[1]);

    OSPData tfColorData = ospNewData(3, OSP_FLOAT3, colors);
    ospSetData(tfn, "colors", tfColorData);
    ospRelease(tfColorData);

    OSPData tfOpacityData = ospNewData(2, OSP_FLOAT, opacities);
    ospSetData(tfn, "opacities", tfOpacityData);
    ospRelease(tfOpacityData);

    ospCommit(tfn);
    ospSetObject(volume, "transferFunction", tfn);
    ospRelease(tfn);

    // Add the volume to our world.
    ospCommit(volume);

    // Set up the camera
    osp::vec3f obj_face = {0.0, 0.0, 0.0};
    osp::vec3f cam_pos  {0.0f, 0.0f, -15.f };
    osp::vec3f cam_up   { 0.f, 1.f, 0.f};
    osp::vec3f cam_view { obj_face.x - cam_pos.x,
                          obj_face.y - cam_pos.y,
                          obj_face.z - cam_pos.z };

    OSPCamera camera = ospNewCamera("perspective");
    ospSet1f(camera, "aspect", ((float) imgSize.x) / ((float) imgSize.y));
    ospSet3f(camera, "pos", cam_pos.x, cam_pos.y, cam_pos.z);
    ospSet3f(camera, "dir", cam_view.x, cam_view.y, cam_view.z);
    ospSet3f(camera, "up", cam_up.x, cam_up.y, cam_up.z);
    ospCommit(camera); 

    // Create the "world" model to contain our volume.
    OSPModel world = ospNewModel();
    ospAddVolume(world, volume);
    ospRelease(volume);
    ospCommit(world);

    // Create ambient lighting
    OSPLight ambientLight = ospNewLight3("ambient");
    ospSet3f(ambientLight, "color", 1.f, 1.f, 1.f);
    ospCommit(ambientLight);
    OSPData lightsData = ospNewData(1, OSP_LIGHT, &ambientLight);
    ospCommit(lightsData);

    // Create OSPRay renderer
    OSPRenderer renderer = ospNewRenderer("scivis");

    ospSetData(renderer, "lights", lightsData);
    ospSetObject(renderer, "camera", camera);
    ospSetObject(renderer, "model", world);
    ospSet1i(renderer, "pixelSamples", 100);
    ospSet1i(renderer, "aoSamples", 100);
    ospSet1f(renderer, "aoIntensity", 10.);
    ospSet3f(renderer, "bgColor", 0., 0., 0.);
    ospCommit(renderer);

    makeMovieFrames(cam_pos, 
                    cam_view, 
                    obj_face, 
                    imgSize, 
                    renderer,
                    camera,
                    2.0);

    // Cleanup remaining objects
    ospRelease(camera);
    ospRelease(lightsData);
    ospRelease(ambientLight);
    ospRelease(world);
    ospRelease(renderer);

    ospShutdown();

  return 0;
}
