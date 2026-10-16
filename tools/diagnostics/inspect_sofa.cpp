#include <iostream>
#include <vector>
#include <set>
#include <cmath>
#include "mysofa.h"

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "config/default_kemar.sofa";
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load(path, &err);
    if (!hrtf) {
        std::cout << "mysofa_load failed: " << err << " for file: " << path << std::endl;
        return 1;
    }

    std::cout << "M (measurements): " << hrtf->M << std::endl;
    std::cout << "R (receivers/ears): " << hrtf->R << std::endl;
    std::cout << "N (samples/taps): " << hrtf->N << std::endl;
    std::cout << "C (coordinates): " << hrtf->C << std::endl;

    std::set<float> radii;
    std::set<float> elevs;
    std::set<float> azims;

    for (int i = 0; i < hrtf->M; i++) {
        float az = std::round(hrtf->SourcePosition.values[i * 3 + 0] * 10.0f) / 10.0f;
        float el = std::round(hrtf->SourcePosition.values[i * 3 + 1] * 10.0f) / 10.0f;
        float r  = std::round(hrtf->SourcePosition.values[i * 3 + 2] * 100.0f) / 100.0f;
        azims.insert(az);
        elevs.insert(el);
        radii.insert(r);
    }

    std::cout << "\nDistinct Radii (" << radii.size() << "): ";
    for (float r : radii) std::cout << r << "m ";
    std::cout << std::endl;

    std::cout << "\nDistinct Elevations (" << elevs.size() << "): ";
    for (float e : elevs) std::cout << e << " deg, ";
    std::cout << std::endl;

    std::cout << "\nAzimuth count: " << azims.size() << " (min=" << *azims.begin() << ", max=" << *azims.rbegin() << ")" << std::endl;

    // Check Receiver Positions (Ears)
    std::cout << "\nReceiver Positions (elements=" << hrtf->ReceiverPosition.elements << "):" << std::endl;
    for (int i = 0; i < hrtf->ReceiverPosition.elements; i++) {
        std::cout << hrtf->ReceiverPosition.values[i] << " ";
    }
    std::cout << std::endl;

    // Check Listener View and Up
    std::cout << "ListenerView: ";
    for (int i = 0; i < hrtf->ListenerView.elements; i++) std::cout << hrtf->ListenerView.values[i] << " ";
    std::cout << std::endl;

    std::cout << "ListenerUp: ";
    for (int i = 0; i < hrtf->ListenerUp.elements; i++) std::cout << hrtf->ListenerUp.values[i] << " ";
    std::cout << std::endl;

    mysofa_free(hrtf);
    return 0;
}
