#include "ItemUseFeedback.h"

namespace ro::item {
UseFeedback GetUseFeedback(unsigned itemId)
{
    UseFeedback result;
    // Ref/GameMode.cpp:12856-13360, including shared-label and range branches.
    switch(itemId) {
    case 501: case 507: case 545: case 569: case 578: case 598:
        result.effectId=204;break;
    case 502: case 579: case 599:
        result.effectId=205;break;
    case 503: case 508: case 546: case 11500:
        result.effectId=206;break;
    case 504: case 509: case 547: case 555: case 556: case 557: case 11501: case 11503:
        result.effectId=207;break;
    case 505: case 510: case 11502: case 11504:
        result.effectId=208;break;
    case 506: case 511:
        result.effectId=209;break;
    case 512: case 513: case 515: case 516: case 531: case 532: case 534: case 535:
    case 548: case 549: case 550: case 574: case 582: case 682:
        result.effectId=210;break;
    case 514: case 533: case 568: case 607: case 680: case 683:
        result.effectId=211;break;
    case 517: case 518: case 519: case 520: case 521: case 526: case 528: case 529:
    case 530: case 536: case 537: case 538: case 539: case 540: case 541: case 542:
    case 543: case 544: case 551: case 552: case 553: case 562: case 563: case 564:
    case 565: case 566: case 567: case 570: case 571: case 572: case 575: case 576:
    case 577: case 580: case 581: case 583: case 584: case 585: case 586: case 663:
        result.effectId=7;break;
    case 645: case 14509:
        result.effectId=218;break;
    case 656: case 14510:
        result.effectId=219;break;
    case 657: case 684: case 14511:
        result.effectId=220;break;
    case 558: case 561: case 12237:
        result.effectId=364;break;
    case 559: case 560: case 11701:
        result.effectId=363;break;
    case 554: case 12122: case 12123: case 12124: case 12195: case 12196: case 12197: case 12198:
    case 12238: case 12239:
        result.effectId=491;break;
    case 685:
        result.effectId=492;break;
    case 662: case 12262:
        result.effectId=507;break;
    case 12018:
        result.effectId=508;break;
    case 12016:
        result.effectId=519;break;
    case 12023:
        result.effectId=545;break;
    case 12017:
        result.effectId=38;break;
    case 12928:
        result.effectId=86;break;
    case 12131:
        result.effectId=612;break;
    case 573: case 596: case 597: case 12234:
        result.effectId=509;break;
    case 12118:
        result.effectId=603;break;
    case 12119:
        result.effectId=51;break;
    case 12120:
        result.effectId=147;break;
    case 12121:
        result.effectId=52;break;
    case 12250: case 12202:
        result.effectId=593;break;
    case 12252: case 12204:
        result.effectId=594;break;
    case 12255: case 12207:
        result.effectId=595;break;
    case 12251: case 12203:
        result.effectId=596;break;
    case 12253: case 12205:
        result.effectId=597;break;
    case 12254: case 12206:
        result.effectId=598;break;
    case 12028:
        result.effectId=576;break;
    case 12029:
        result.effectId=577;break;
    case 12030:
        result.effectId=578;break;
    case 12031:
        result.effectId=579;break;
    case 12032:
        result.effectId=580;break;
    case 12033:
        result.effectId=581;break;
    case 14546:
        result.effectId=682;break;
    case 14547:
        result.effectId=683;break;
    case 14548:
        result.effectId=684;break;
    case 14549:
        result.effectId=685;break;
    case 14550:
        result.effectId=686;break;
    default:break;
    }
    if((itemId>=12041&&itemId<=12045)||(itemId>=12071&&itemId<=12075))result.effectId=593;
    if((itemId>=12046&&itemId<=12050)||(itemId>=12076&&itemId<=12080))result.effectId=594;
    if((itemId>=12051&&itemId<=12055)||(itemId>=12081&&itemId<=12085))result.effectId=595;
    if((itemId>=12056&&itemId<=12060)||(itemId>=12086&&itemId<=12090))result.effectId=596;
    if((itemId>=12061&&itemId<=12065)||(itemId>=12091&&itemId<=12095))result.effectId=597;
    if((itemId>=12066&&itemId<=12070)||(itemId>=12096&&itemId<=12100))result.effectId=598;
    if((itemId>=12310&&itemId<=12311))result.effectId=699;
    switch(itemId) {
    case 522:
        result.sound="_heal_effect.wav";break;
    case 523:
        result.sound="effect\\EF_Blessing.wav";break;
    case 12027:
        result.sound="wraith_die.wav";break;
    case 12034:
        result.sound="ork_warrior_breath.wav";break;
    case 12122: case 12123: case 12124: case 12195: case 12196: case 12197: case 12198: case 12238: case 12239: case 554:
        result.sound="_heal_effect.wav";break;
    case 12118: case 12119: case 12120: case 12121:
        result.sound="evil_cloud_hermit_attack.wav";break;
    case 12018:
        result.sound="effect\\itempokjuk.wav";break;
    case 12023:
        result.sound="effect\\ro2effect.wav";break;
    case 12016:
        result.sound="effect\\fast.wav";break;
    case 662: case 12262:
        result.sound="effect\\mapae.wav";break;
    default:break;
    }
    if(itemId==684)result.extraEffectId=261;
    switch(itemId) {
    case 12208:result.noticeMessageId=1249;break;
    case 14532:result.noticeMessageId=1384;break;
    case 14533:result.noticeMessageId=1385;break;
    case 12312:result.noticeMessageId=1386;break;
    default:break;
    }
    return result;
}
}
