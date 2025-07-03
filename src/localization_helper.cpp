#include "localization_helper.h"

#include "cstdint"

void updateVelocity(int sensorSrc, int sensorDest, int digit, int decimal) {
}

int initTrainVelocityTrackA(int trainId, int speed) {
    switch (speed) {
        case 14: {
            switch (trainId) {
                case 14:
                    return 0;
                    break;
                case 15:
                    return 0;
                    break;
                case 16:
                    return 0;
                    break;
                case 17:
                    return 0;
                    break;
                case 18:
                    return 0;
                    break;
                case 55:
                    return 0;
                    break;

                default:
                    return 0;
                    break;
            }
        }

        case 8: {
            switch (trainId) {
                case 14:
                    return 0;
                    break;
                case 15:
                    return 0;
                    break;
                case 16:
                    return 0;
                    break;
                case 17:
                    return 0;
                    break;
                case 18:
                    return 0;
                    break;
                case 55:
                    return 0;
                    break;

                default:
                    return 0;
                    break;
            }
        }
        default:
            return 0;
            break;
    }
}