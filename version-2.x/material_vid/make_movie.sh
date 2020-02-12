#!/bin/bash
ffmpeg -framerate 25 -i frames/frame_%d.ppm -c:v libx264 -profile:v high -crf 20 -pix_fmt yuv420p output.mp4
