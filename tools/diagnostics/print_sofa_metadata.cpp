#include <iostream>
#include "mysofa.h"

int main() {
    int err = 0;
    const char* path = "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa";
    struct MYSOFA_HRTF* hrtf = mysofa_load(path, &err);
    if (!hrtf) {
        std::cout << "Failed to load SOFA: " << err << std::endl;
        return 1;
    }

    std::cout << "=== SOFA Global Attributes ===" << std::endl;
    struct MYSOFA_ATTRIBUTE* attr = hrtf->attributes;
    while (attr) {
        std::cout << "  " << (attr->name ? attr->name : "(null)") << ": " 
                  << (attr->value ? attr->value : "(null)") << std::endl;
        attr = attr->next;
    }

    std::cout << "\n=== SourcePosition Attributes ===" << std::endl;
    attr = hrtf->SourcePosition.attributes;
    while (attr) {
        std::cout << "  " << (attr->name ? attr->name : "(null)") << ": " 
                  << (attr->value ? attr->value : "(null)") << std::endl;
        attr = attr->next;
    }

    std::cout << "\n=== DataSamplingRate ===" << std::endl;
    if (hrtf->DataSamplingRate.values && hrtf->DataSamplingRate.elements > 0) {
        std::cout << "  Rate: " << hrtf->DataSamplingRate.values[0] << " Hz" << std::endl;
    }

    mysofa_free(hrtf);
    return 0;
}
