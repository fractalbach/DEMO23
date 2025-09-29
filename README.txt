3D Software Render of a Diablo Model
======================================================================

About
------------------------------------------------------------

3D Cpu Rasterizer in C



Files
------------------------------------------------------------

demo23.c  - main source code file
build.sh - shell script to compile the code

model/diablo3_pose.obj - the diablo model that is rendered
model/gen.py           - script that processes the obj file
model/model.c          - output from gen.py, included into demo23.c

model/texture/diablo3_pose_diffuse.tga - texture image
model/texture/main.cpp                 - processes the tga file 
model/texture/diffuse.c                - output from main.cpp

model/texture/tgaimage.cpp - borrowed from the internet
model/texture/tgaimage.h   - borrowed from the internet



Arrow key controls
------------------------------------------------------------

Up    : Wireframe Mode
Right : Lighting Only
Left  : Texture Only
Down  : Texture + Lighting (Default)
