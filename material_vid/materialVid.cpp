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
void makeMovieFrames(osp::vec3f cam_pos,
                     osp::vec3f cam_view, 
                     osp::vec3f obj_face, 
                     osp::vec2i imgSize, 
                     OSPRenderer renderer,
                     OSPCamera camera,
                     float stepSize = 1.0)
{
    OSPFrameBuffer framebuffer = ospNewFrameBuffer(imgSize, OSP_FB_SRGBA, 
        OSP_FB_COLOR | /*OSP_FB_DEPTH |*/ OSP_FB_ACCUM);
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


int main(int argc, const char **argv) {

    OSPError initError = ospInit(&argc, argv);

    if (initError != OSP_NO_ERROR)
        return initError;

    ospDeviceSetErrorFunc(
        ospGetCurrentDevice(), [](OSPError error, const char *errorDetails) {
            std::cerr << "OSPRay error: " << errorDetails << std::endl;
            exit(error);
        });


    // image size
    osp::vec2i imgSize;
    imgSize.x = 1024; // width
    imgSize.y = 768; // height

    osp::vec3f obj_face {0.0, 0.0, 0.0};

    // camera
    osp::vec3f cam_pos  { 0.0f, 0.0f, -5.0f};
    osp::vec3f cam_up   { 0.f, 1.f, 0.f};
    osp::vec3f cam_view { obj_face.x - cam_pos.x,
                          obj_face.y - cam_pos.y,
                          obj_face.z - cam_pos.z };

    // triangle mesh data
    int numVertex = 8;
    float vertex[] = { 
                       -0.5f, -0.5f, 0.5f, 
                        0.5f, -0.5f, 0.5f, 
                        0.5f,  0.5f, 0.5f,
                       -0.5f,  0.5f, 0.5f,
                       -0.5f, .5f, -.5f, 
                       .5f, .5f, -.5f, 
                       .5f, -.5f, -.5f, 
                       -.5f, -.5f, -.5f
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


    // Create and setup camera
    OSPCamera camera = ospNewCamera("perspective");
    ospSet1f(camera, "aspect", ((float) imgSize.x) / ((float) imgSize.y));
    ospSet3f(camera, "pos", cam_pos.x, cam_pos.y, cam_pos.z);
    ospSet3f(camera, "dir", cam_view.x, cam_view.y, cam_view.z);
    ospSet3f(camera, "up", cam_up.x, cam_up.y, cam_up.z);
    ospCommit(camera);

    // Create and setup model and mesh
    OSPGeometry mesh = ospNewGeometry("triangles");

    OSPData vertexData = ospNewData(numVertex, OSP_FLOAT3, vertex);
    ospCommit(vertexData);
    ospSetData(mesh, "vertex", vertexData);    
    ospRelease(vertexData);

    OSPData colorData = ospNewData(numVertex, OSP_FLOAT4, color);
    ospCommit(colorData);
    ospSetData(mesh, "vertex.color", colorData);
    ospRelease(colorData);

    OSPData indexData = ospNewData(numIdx, OSP_INT3, index);
    ospCommit(indexData);
    ospSetData(mesh, "index", indexData);
    ospRelease(indexData);

    // Create a material to associate with our geometry.
    OSPMaterial material = ospNewMaterial2("pathtracer", "ThinGlass");
    ospSet1f(material, "attenuationDistance", 0.2f);
    ospCommit(material);
    ospSetMaterial(mesh, material);

    ospCommit(mesh);

    // World creation
    OSPModel world = ospNewModel();
    ospAddGeometry(world, mesh);
    ospRelease(mesh);
    ospCommit(world);

    // Create and setup light for Ambient Occlusion
    OSPLight ambientLight = ospNewLight3("ambient");
    ospSet3f(ambientLight, "color", 1.f, 1.f, 1.f);
    ospCommit(ambientLight);
    OSPData lights = ospNewData(1, OSP_LIGHT, &ambientLight);
    ospCommit(lights);

    // Create renderer
    OSPRenderer renderer = ospNewRenderer("pathtracer");

    ospSet1i(renderer, "aoSamples", 100);
    ospSet1i(renderer, "aoIntensity", 10);
    ospSet3f(renderer, "bgColor", 1.0, 1.0, 1.0);
    ospSetObject(renderer, "model", world);
    ospSetObject(renderer, "camera", camera);
    ospSetObject(renderer, "lights", lights);
    ospCommit(renderer);

    makeMovieFrames(cam_pos, 
                    cam_view, 
                    obj_face, 
                    imgSize, 
                    renderer,
                    camera,
                    .8);

    // Final cleanups
    ospRelease(renderer);
    ospRelease(camera);
    ospRelease(lights);
    ospRelease(world);
    ospRelease(material);

    ospShutdown();

    return 0;
}
