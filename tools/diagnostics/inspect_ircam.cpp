#include <iostream>
#include <set>
#include <string>
#include "mysofa.h"

int main() {
    int err = 0;
    const char* path = "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa";
    struct MYSOFA_HRTF* hrtf = mysofa_load(path, &err);
    if (!hrtf) { std::cout << "Failed: " << err << std::endl; return 1; }
    std::cout << "IRCAM LISTEN: M=" << hrtf->M << " R=" << hrtf->R << " N=" << hrtf->N << std::endl;
    struct MYSOFA_ATTRIBUTE* attr = hrtf->attributes;
    while (attr) {
        if (attr->name) {
            std::string n(attr->name);
            if (n.find("Title") != std::string::npos ||
                n.find("Database") != std::string::npos ||
                n.find("Comment") != std::string::npos ||
                n.find("Listener") != std::string::npos) {
                std::cout << "  " << attr->name << ": " << (attr->value ? attr->value : "") << std::endl;
            }
        }
        attr = attr->next;
    }
    std::set<float> elevs;
    for (int i = 0; i < (int)hrtf->M; i++) {
        elevs.insert(hrtf->SourcePosition.values[i * 3 + 1]);
    }
    std::cout << "Distinct elevations (" << elevs.size() << "): ";
    for (float e : elevs) std::cout << e << " ";
    std::cout << std::endl;
    mysofa_free(hrtf);
    return 0;
}
