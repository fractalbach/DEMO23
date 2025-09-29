#include <iostream>
#include "tgaimage.cpp"

const int width = 1024;
const int height = 1024;
TGAImage img = TGAImage();

int main() {
    img.read_tga_file("diablo3_pose_diffuse.tga");

    std::cout 
        << "const uint32_t model_diffuse" 
        << "[" << height << "]"
        << "[" << width << "]"
        << " = {\n\t";

    for (int y=0; y<height; y++) {
        std::cout << "{";
        for (int x=0; x<width; x++) {
            TGAColor color = img.get(x, y);
            uint32_t rgb = (color[2] << 16) | (color[1] << 8) | (color[0]);
            std::cout << "0x" << std::hex << rgb << ",";
        }
        std::cout << "},";
    }

    std::cout << "\n};";
    
    return 0;
}