// Ref CSession::GetSkillActionInfo2 — decompiled from Ref/Session2.cpp (0x67aac0).
#include "SkillActionInfo2.h"

#include <algorithm>

static void ApplyBeginEffect12Property(int property, int& beginEffectId)
{
    switch (property) {
    case 1:
        beginEffectId = 54;
        break;
    case 2:
        beginEffectId = 56;
        break;
    case 3:
        beginEffectId = 55;
        break;
    case 4:
        beginEffectId = 57;
        break;
    case 5:
        beginEffectId = 59;
        break;
    case 6:
        beginEffectId = 58;
        break;
    default:
        break;
    }
}

void GetSkillActionInfo2(int skillId, int& beginEffectId, int& motionType, int property, int job)
{
    beginEffectId = 16;
    motionType = 8;

    if (skillId <= 379) {
        if (skillId == 379) {
            beginEffectId = 59;
        } else {
            switch (skillId) {
            case 5:
            case 7:
            case 42:
            case 46:
            case 110:
                motionType = 2;
                break;
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 20:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 30:
            case 31:
            case 32:
            case 33:
            case 35:
            case 54:
            case 66:
            case 67:
            case 68:
            case 69:
            case 70:
            case 72:
            case 73:
            case 74:
            case 75:
            case 76:
            case 77:
            case 78:
            case 79:
            case 80:
            case 81:
            case 82:
            case 83:
            case 84:
            case 85:
            case 86:
            case 87:
            case 88:
            case 89:
            case 90:
            case 91:
            case 92:
            case 108:
            case 156:
            case 157:
            case 254:
            case 255:
            case 256:
            case 261:
            case 262:
            case 266:
            case 267:
            case 277:
            case 279:
            case 280:
            case 281:
            case 282:
            case 283:
            case 285:
            case 286:
            case 287:
            case 288:
            case 289:
            case 290:
            case 339:
            case 340:
            case 341:
            case 342:
            case 365:
            case 373:
            case 374:
            case 375:
                beginEffectId = 12;
                break;
            case 29:
                beginEffectId = 12;
                motionType = 7;
                break;
            case 34:
                beginEffectId = 12;
                motionType = 0;
                break;
            case 45:
                beginEffectId = 43;
                break;
            case 47:
            case 56:
            case 58:
            case 62:
                beginEffectId = -1;
                motionType = 2;
                break;
            case 51:
            case 55:
            case 60:
            case 63:
            case 64:
            case 93:
            case 158:
            case 159:
            case 160:
            case 161:
            case 162:
            case 163:
            case 164:
            case 165:
            case 166:
            case 167:
            case 168:
            case 169:
            case 170:
            case 171:
            case 172:
            case 173:
            case 174:
            case 175:
            case 176:
            case 177:
            case 178:
            case 179:
            case 180:
            case 181:
            case 182:
            case 183:
            case 184:
            case 185:
            case 186:
            case 187:
            case 188:
            case 189:
            case 190:
            case 191:
            case 192:
            case 258:
            case 272:
            case 273:
            case 338:
            case 347:
            case 368:
            case 370:
            case 371:
            case 372:
                beginEffectId = -1;
                break;
            case 57:
                beginEffectId = 144;
                motionType = 2;
                break;
            case 59:
                beginEffectId = -1;
                motionType = 12;
                break;
            case 61:
            case 152:
                beginEffectId = -1;
                motionType = 11;
                break;
            case 115:
            case 116:
            case 117:
            case 118:
            case 119:
            case 120:
            case 121:
            case 122:
            case 123:
            case 124:
            case 125:
                motionType = 5;
                break;
            case 136:
                beginEffectId = -1;
                motionType = 7;
                break;
            case 141:
            case 344:
            case 345:
            case 346:
            case 350:
                beginEffectId = 59;
                break;
            case 149:
                motionType = 11;
                break;
            case 151:
                beginEffectId = -1;
                motionType = 5; // Ref: goto merged label for motion 5
                break;
            case 196:
            case 209:
            case 249:
            case 270:
            case 349:
                motionType = 0;
                beginEffectId = -1;
                break;
            case 230:
            case 231:
            case 232:
            case 234:
            case 235:
            case 236:
            case 237:
            case 268:
                motionType = 0;
                beginEffectId = 12;
                break;
            case 243:
            case 244:
            case 247:
                motionType = 0;
                beginEffectId = 342;
                break;
            case 271:
                // Ref's LOBYTE mask maps non-4016 jobs to signed value -182.
                beginEffectId = (job == 4016) ? 510 : 328;
                break;
            case 334:
            case 336:
                beginEffectId = 342;
                break;
            case 335:
                beginEffectId = 343;
                break;
            case 361:
            case 362:
            case 367:
                beginEffectId = 58;
                break;
            default:
                break;
            }
        }
        if (beginEffectId == 12) {
            ApplyBeginEffect12Property(property, beginEffectId);
        }
        return;
    }

    if (skillId <= 499) {
        if (skillId == 499) {
            motionType = 2;
        } else {
            switch (skillId) {
            case 382:
            case 394:
                motionType = 18;
                beginEffectId = 58;
                break;
            case 383:
            case 480:
            case 483:
            case 489:
                beginEffectId = 58;
                break;
            case 389:
                motionType = 0;
                beginEffectId = 501;
                break;
            case 397:
                beginEffectId = 502;
                motionType = 11;
                break;
            case 398:
            case 399:
            case 495:
                beginEffectId = -1;
                break;
            case 400:
            case 401:
            case 402:
            case 403:
            case 405:
            case 482:
            case 484:
                beginEffectId = 12;
                break;
            case 406:
                beginEffectId = 59;
                break;
            case 408:
            case 409:
            case 410:
                beginEffectId = 342;
                break;
            case 411:
            case 427:
            case 434:
            case 444:
            case 445:
            case 447:
            case 448:
            case 449:
            case 450:
            case 451:
            case 452:
            case 453:
            case 454:
            case 455:
            case 456:
            case 457:
            case 458:
            case 460:
            case 461:
            case 462:
            case 463:
            case 464:
            case 465:
            case 467:
            case 468:
            case 469:
            case 470:
            case 471:
            case 472:
            case 494:
                beginEffectId = 441;
                break;
            case 425:
            case 493:
                motionType = 0;
                beginEffectId = 441;
                break;
            case 426:
                motionType = 19;
                beginEffectId = -1;
                break;
            case 446:
                motionType = 12;
                beginEffectId = 12;
                break;
            case 475:
                motionType = 0;
                beginEffectId = 59;
                break;
            case 478:
            case 490:
                motionType = 28;
                beginEffectId = 12;
                break;
            case 479:
                motionType = 0;
                beginEffectId = 12;
                break;
            case 496:
                motionType = 7;
                beginEffectId = 573;
                break;
            case 497:
                motionType = 7;
                beginEffectId = 574;
                break;
            case 498:
                motionType = 7;
                beginEffectId = 575;
                break;
            default:
                if (beginEffectId == 12) {
                    ApplyBeginEffect12Property(property, beginEffectId);
                }
                return;
            }
        }
        if (beginEffectId == 12) {
            ApplyBeginEffect12Property(property, beginEffectId);
        }
        return;
    }

    if (skillId <= 688) {
        if (skillId == 688) {
            motionType = 5;
        } else {
            switch (skillId) {
            case 503:
            case 504:
            case 505:
            case 512:
            case 513:
            case 514:
            case 515:
            case 517:
                motionType = 41;
                beginEffectId = 441;
                break;
            case 518:
            case 519:
            case 520:
            case 521:
                motionType = 42;
                beginEffectId = 441;
                break;
            case 525:
            case 532:
            case 543:
                motionType = 37;
                beginEffectId = 441;
                break;
            case 534:
            case 535:
            case 536:
                motionType = 37;
                beginEffectId = 55;
                break;
            case 537:
            case 538:
            case 539:
                motionType = 37;
                beginEffectId = 54;
                break;
            case 540:
            case 541:
            case 542:
                motionType = 37;
                beginEffectId = 57;
                break;
            case 555:
                motionType = 0;
                beginEffectId = 501;
                break;
            case 572:
            case 573:
            case 574:
            case 575:
                beginEffectId = 441;
                break;
            case 654:
            case 663:
            case 666:
            case 667:
            case 668:
            case 675:
            case 676:
            case 678:
                beginEffectId = -1;
                break;
            case 661:
                beginEffectId = 59;
                break;
            case 669:
                beginEffectId = 12;
                break;
            default:
                if (beginEffectId == 12) {
                    ApplyBeginEffect12Property(property, beginEffectId);
                }
                return;
            }
        }
        if (beginEffectId == 12) {
            ApplyBeginEffect12Property(property, beginEffectId);
        }
        return;
    }

    if (skillId > 1010) {
        if (skillId > 1017) {
            if (skillId > 8220) {
                if (skillId == 8222) {
                    beginEffectId = 12;
                }
            } else {
                switch (skillId) {
                case 8220:
                    motionType = 0;
                    beginEffectId = -1;
                    break;
                case 1018:
                    beginEffectId = 55;
                    break;
                case 1019:
                    beginEffectId = 57;
                    break;
                default:
                    break;
                }
            }
        } else if (skillId == 1017) {
            beginEffectId = 56;
        } else {
            switch (skillId) {
            case 1011:
                motionType = 16;
                beginEffectId = -1;
                break;
            case 1014:
                motionType = 0;
                beginEffectId = 58;
                break;
            case 1015:
                beginEffectId = 12;
                break;
            case 1016:
                beginEffectId = -1;
                break;
            default:
                break;
            }
        }
    } else {
        if (skillId == 1010) {
            motionType = 0;
            beginEffectId = 58;
        } else if (skillId <= 1001) {
            if (skillId != 1001) {
                switch (skillId) {
                case 689:
                    beginEffectId = 12;
                    motionType = 0;
                    break;
                case 690:
                    beginEffectId = 12;
                    motionType = 7;
                    break;
                case 691:
                    beginEffectId = 58;
                    break;
                case 694:
                    beginEffectId = 12;
                    break;
                default:
                    if (beginEffectId == 12) {
                        ApplyBeginEffect12Property(property, beginEffectId);
                    }
                    return;
                }
            } else {
                motionType = 0;
                beginEffectId = 58;
            }
        } else {
            switch (skillId) {
            case 1004:
                beginEffectId = -1;
                motionType = 11;
                break;
            case 1006:
                beginEffectId = 12;
                break;
            case 1008:
                beginEffectId = 54;
                break;
            case 1009:
                motionType = 2;
                break;
            default:
                break;
            }
        }
    }

    if (beginEffectId == 12) {
        ApplyBeginEffect12Property(property, beginEffectId);
    }
}
