#!/usr/bin/env python3
"""Generate explicit Ref effect case tables from the committed effect index."""
from __future__ import annotations

import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INDEX = ROOT / "docs/skill/generated/effect_handler_index.tsv"
COVERAGE = ROOT / "docs/skill/generated/EFFECT_COVERAGE.tsv"
OUT = ROOT / "src/world"

FORCE_UNPORTED = {244, 537, 612}
FORCE_ROUTE = {315, 366, 376, 262, 264, 266, 275, 276, 267, 606, 607, 547, 553, 555, 429, 430, 431, 491, 492, 507, 508, 565, 566, 567, 568, 569,
               593, 594, 595, 596, 597, 598, 603, 608, 609, 646, 649,
               668, 669, 670, 677, 699, 67, 87, 133, 134, 136, 251, 406, 407,
               65, 66, 69, 75, 76, 77, 78, 83, 84, 85, 88, 91, 96, 97, 103,
               106, 107, 108, 111, 113, 124, 127, 129, 131, 153, 154, 155,
               130, 250, 204, 205, 206, 207, 210, 234, 245, 255, 256, 257, 258, 390, 391,
               95, 102, 570, 571, 576, 577, 578, 579, 580, 583, 585, 586, 587, 588, 589,
               630, 631, 632, 633, 634, 637, 638, 639, 640, 641, 642, 643, 644, 645, 647, 648,
               200, 201, 202, 203, 226, 232, 249, 254, 274, 336, 362, 377, 384, 386, 388, 393, 395, 397, 398, 399, 400, 408, 409, 418, 450, 453, 455, 485, 486, 494, 495, 496, 518, 520, 537, 543, 546, 572, 573, 574, 575, 599, 602, 604, 666, 675, 678, 679, 680, 682, 683, 684, 685, 686, 689, 690, 691, 692, 693, 694, 696, 701, 702, 703, 506, 502, 505, 552, 556}
FORCE_ROUTE.update({7, 11, 15, 21, 60, 80, 82, 93, 94, 99, 100, 114, 115, 117, 119, 120, 137, 140, 166, 198, 328, 330, 378, 380, 383, 389, 411, 416, 444, 451, 452, 456, 503, 510, 617})
FORCE_ROUTE.update(range(475, 485))
FORCE_ROUTE.add(273)
FORCE_ROUTE.add(302)
FORCE_ROUTE.add(333)
FORCE_ROUTE.add(6)
FORCE_ROUTE.add(8)
FORCE_ROUTE.add(9)
FORCE_ROUTE.add(90)
FORCE_ROUTE.add(74)
FORCE_ROUTE.add(13)
FORCE_ROUTE.add(92)
FORCE_ROUTE.add(361)
FORCE_ROUTE.add(365)
FORCE_ROUTE.add(440)

DIRECT_CALLS = {
    # Main OnProcess cases whose index handler includes more than raw STR.
    105: "return true;",
    161: "return true;",
    443: "return true;",
    489: "return true;",
    490: "return true;",
    116: "return ProcessWaterBall116();",
    118: "return ProcessFireIvy118();",
    138: "return ProcessFirePillar138();",
    165: "return ProcessBanjjakii165();",
    41: "return ProcessAngelus();",
    336: "return ProcessGuardEffects();",
    496: "return ProcessGuardEffects();",
    453: "return ProcessNpcStopEffects();",
    604: "return ProcessNpcStopEffects();",
    675: "return ProcessGuardEffects();",
    573: "return ProcessTwilightEffects();",
    574: "return ProcessTwilightEffects();",
    575: "return ProcessTwilightEffects();",
    518: "return ProcessParticleSpreadEffects();",
    543: "return ProcessParticleTimeEffects();",
    546: "return ProcessParticleTimeEffects();",
    572: "return ProcessParticleTimeEffects();",
    408: "return ProcessSphereWind2Effect();",
    409: "return ProcessSoulBreaker2Effect();",
    411: "return ProcessPeongEffects();",
    416: "return ProcessAirTexture416();",
    556: "return ProcessParticlePath556();",
    537: "return ProcessEffect537();",
    599: "return ProcessEffect599();",
    602: "return ProcessEffect602();",
    273: "return ProcessEffect273();",
    302: "return ProcessDemonstration302();",
    333: "return ProcessMaple333();",
    6: "return ProcessEntry6();",
    8: "return ProcessWarp8();",
    9: "return ProcessEnhance9();",
    90: "return ProcessLord90();",
    74: "return ProcessIceWall74();",
    13: "return ProcessGlassWall13();",
    92: "return ProcessMeteorStorm92();",
    361: "return ProcessSoulBreaker361Effect();",
    365: "return ProcessEffect365();",
    440: "return ProcessEffect440();",
    444: "return ProcessRunStop444();",
    456: "return ProcessCartBoost456();",
    7: "return ProcessExit7();",
    11: "return ProcessEndure11();",
    15: "return ProcessSoulStrike15(0);",
    21: "return ProcessDetoxication21();",
    60: "SpawnLockOn(); return true;",
    80: "SpawnSpearBmr(); return true;",
    82: "SpawnTurnUndead(); return true;",
    93: "return ProcessYufitel93();",
    94: "return ProcessYufitelHit(0);",
    99: "return ProcessFlasher99();",
    100: "return ProcessRemoveTrap100();",
    115: "return ProcessBlitzBeat115();",
    117: "SpawnWaterBall2(); return true;",
    140: "return ProcessRevive140();",
    198: "return true;",
    380: "return ProcessEffect380();",
    120: "return ProcessCloaking120();",
    119: "SpawnDetecting(); return true;",
    114: "return ProcessBottom114();",
    137: "return ProcessBottom2Effect137();",
    452: "return ProcessYufitelHit(1);",
    451: "return ProcessSoulStrike15(1);",
    328: "return ProcessBeginAsuraEffect();",
    330: "return ProcessHitLineEffect(0, false);",
    510: "return ProcessBeginAsuraEffect();",
    503: "return ProcessBeginAsura2Effect();",
    378: "return ProcessEnergyDrain2Effect();",
    383: "return ProcessEnergyDrain3Effect();",
    389: "return ProcessPortalWind389();",
    166: "return ProcessMakeBlur();",
    617: "return ProcessMakeBlur2();",
    49: "return ProcessFireHit();",
    67: "return ProcessProvoke();",
    87: "return ProcessLexDivina();",
    133: "return ProcessStrTerminalEffect();",
    134: "return ProcessStrTerminalEffect();",
    136: "ProcessEZ2STR(); return true;",
    251: "return ProcessEZ2STR(0.0f, -20.0f, 0.0f);",
    406: "ProcessEZ2STR(); return true;",
    407: "ProcessEZ2STR(); return true;",
    65: "return ProcessClipWaveStrEffect();",
    66: "return ProcessClipWaveStrEffect();",
    69: "return ProcessClipWaveStrEffect();",
    75: "return ProcessClipWaveStrEffect();",
    76: "return ProcessClipWaveStrEffect();",
    77: "return ProcessClipWaveStrEffect();",
    78: "return ProcessClipWaveStrEffect();",
    83: "return ProcessClipWaveStrEffect();",
    84: "return ProcessClipWaveStrEffect();",
    85: "return ProcessClipWaveStrEffect();",
    88: "return ProcessClipWaveStrEffect();",
    91: "return ProcessClipWaveStrEffect();",
    96: "return ProcessClipWaveStrEffect();",
    97: "return ProcessClipWaveStrEffect();",
    103: "return ProcessClipWaveStrEffect();",
    106: "return ProcessClipWaveStrEffect();",
    107: "return ProcessClipWaveStrEffect();",
    108: "return ProcessClipWaveStrEffect();",
    111: "return ProcessClipWaveStrEffect();",
    113: "return ProcessClipWaveStrEffect();",
    124: "return ProcessClipWaveStrEffect();",
    127: "return ProcessClipWaveStrEffect();",
    129: "return ProcessClipWaveStrEffect();",
    131: "return ProcessClipWaveStrEffect();",
    153: "return ProcessClipWaveStrEffect();",
    154: "return ProcessClipWaveStrEffect();",
    155: "return ProcessClipWaveStrEffect();",
    130: "return ProcessClipWaveStrEffect();",
    250: "return ProcessClipWaveStrEffect();",
    204: "return ProcessClipWaveStrEffect();",
    205: "return ProcessClipWaveStrEffect();",
    206: "return ProcessClipWaveStrEffect();",
    207: "return ProcessClipWaveStrEffect();",
    210: "return ProcessClipWaveStrEffect();",
    234: "return ProcessClipWaveStrEffect();",
    245: "return ProcessClipWaveStrEffect();",
    255: "return ProcessClipWaveStrEffect();",
    256: "return ProcessClipWaveStrEffect();",
    257: "return ProcessClipWaveStrEffect();",
    258: "return ProcessClipWaveStrEffect();",
    390: "return ProcessClipWaveStrEffect();",
    391: "return ProcessClipWaveStrEffect();",
    95: "return ProcessClipWaveStrEffect();",
    102: "return ProcessClipWaveStrEffect();",
    159: "SpawnTopRank(); return true;",
    160: "SpawnParty(); return true;",
    162: "SpawnSnow(); return true;",
    163: "SpawnSakura(); return true;",
    164: "SpawnStatusState(); return true;",
    183: "return ProcessHit2Blue();",
    170: "return ApplyCartRevolution();",
    171: "ApplyVenomDust2(); return true;",
    196: "ApplyCurseAttack(); return true;",
    212: "SpawnDarkBreath(); return true;",
    215: "SpawnSummonSlave(); return true;",
    216: "SpawnBloodDrain(); return true;",
    217: "SpawnEnergyDrain(); return true;",
    218: "return ProcessPotionConcentration();",
    219: "return ProcessPotionConcentration();",
    248: 'ApplyProvidence("effect\\\\ring_yellow.tga"); ProcessEZ2STR(); return FinishEffectFrame();',
    253: "return ProcessHealBlue();",
    12: 'ApplyPairedSaintCasting("effect\\\\ring_yellow.tga", 1, true, 0.0f); return true;',
    278: 'ApplyBottomMusic("effect\\\\zz.bmp", 1); return true;',
    279: 'ApplyBottomMusic("effect\\\\pocket.bmp", 3); return true;',
    282: 'ApplyBottomMusic("effect\\\\twirl.bmp", 7); return true;',
    283: 'ApplyBottomOut("effect\\\\safeline.bmp", 0); return true;',
    285: 'ApplyBottomLight("effect\\\\twirl.bmp", 4); return true;',
    286: 'ApplyBottomMusic("effect\\\\melody_b.bmp", 0); return true;',
    289: 'ApplyBottomMusic("effect\\\\idun_apple.bmp", 6); return true;',
    291: 'ApplyBottomMusic("effect\\\\melody_a.bmp", 0); return true;',
    293: 'ApplyBottomMusic("effect\\\\kiss.bmp", 4); return true;',
    298: "return ProcessThrowItemEffect();",
    299: "return ProcessThrowItemEffect();",
    392: "ApplyKyrieGuard(); return FinishEffectFrame();",
    394: 'for (short windTime = 0; windTime < 45; windTime = static_cast<short>(windTime + 9)) ApplySphereWind("effect\\\\fire_ivy.bmp", windTime, 1); return true;',
    402: "ApplyPortalWind402(); return true;",
    405: 'ApplyBottomMagnus("effect\\\\ring_white.tga", 2); return true;',
    441: 'ApplyPairedSaintCasting("effect\\\\ring_blue.tga", 5, true, -100.0f); return true;',
    454: 'ApplyPairedSaintCasting("effect\\\\ring_black.tga", 6, true, 0.0f); return true;',
    487: 'ApplyBlackDevil("effect\\\\ring_black.tga"); return true;',
    491: "ProcessEZ2STR(); return true;",
    492: "ProcessEZ2STR(); return true;",
    541: "return ProcessThrowItemEffect();",
    564: 'ApplyCoupleCasting("effect\\\\ring_red.tga"); return true;',
    565: "return ProcessTier3StrEffect();",
    566: "return ProcessTier3StrEffect();",
    567: "return ProcessTier3StrEffect();",
    568: "return ProcessTier3StrEffect();",
    569: "return ProcessTier3StrEffect();",
    600: "return ProcessThrowItemEffect();",
    200: "return ProcessLevel99Effect();",
    397: "return ProcessLevel99Effect();",
    201: "return ProcessLevel99_2Effect();",
    398: "return ProcessLevel99_2Effect();",
    202: "return ProcessLevel99_3Effect();",
    362: "return ProcessLevel99_3Effect();",
    678: "return ProcessLevel99_3Effect();",
    692: "return ProcessLevel99_3Effect();",
    203: "return ProcessGumGangEffect();",
    254: "return ProcessGumGangEffect();",
    418: "return ProcessGumGangEffect();",
    455: "return ProcessGumGangEffect();",
    485: "return ProcessGumGangEffect();",
    486: "return ProcessGumGangEffect();",
    377: "return ProcessBash3DEffect();",
    386: "return ProcessBash3DEffect();",
    399: "return ProcessBash3DEffect();",
    400: "return ProcessBash3DEffect();",
    376: "return ProcessHitLineEffect();",
    429: "return ProcessHitLine4Effect();",
    430: "return ProcessHitLine5Effect();",
    431: "return ProcessHitLine3Effect();",
    262: "return ProcessTeiHitEffects();",
    264: "return ProcessTeiHitEffects();",
    266: "return ProcessTeiHitEffects();",
    275: "return ProcessTeiHitEffects();",
    276: "return ProcessTeiHitEffects();",
    267: "return ProcessChimtoEffect(0);",
    606: "return ProcessChimtoEffect(2);",
    607: "return ProcessChimtoEffect(1);",
    547: "return ProcessStinSmaEffects();",
    553: "return ProcessStinSmaEffects();",
    555: "return ProcessStinSmaEffects();",
    249: "return ProcessShieldBoomerangEffects();",
    494: "return ProcessShieldBoomerangEffects();",
    520: "return ProcessShieldBoomerangEffects();",
    274: "return ProcessRgCoinEffects();",
    495: "return ProcessRgCoinEffects();",
    226: "return ProcessGrandCrossEffects();",
    450: "return ProcessGrandCrossEffects();",
    232: "return ProcessLineLinkEffects();",
    384: "return ProcessLineLinkEffects();",
    395: "return ProcessLineLinkEffects();",
    329: "return ProcessTripleAttackEffects();",
    388: "return ProcessTripleAttackEffects();",
    393: "return ProcessTripleAttackEffects();",
    610: "return ProcessTarotEffect();",
    611: "return ProcessTarotEffect();",
    613: "return ProcessThrowItemEffect();",
    614: "return ProcessThrowItemEffect();",
    615: "return ProcessThrowItemEffect();",
    616: "return ProcessThrowItemEffect();",
    676: "return ProcessTarotEffect();",
    679: "return ProcessLevel99Effect();",
    680: "return ProcessLevel99_2Effect();",
    689: "return ProcessEffectTextureAnimation();",
    690: "return ProcessEffectTextureAnimation();",
    691: "return ProcessEffectTextureAnimation();",
    696: "return ProcessEffectTextureAnimation();",
    693: "return ProcessEffectTextureSet();",
    694: "return ProcessEffectTextureSet();",
    701: "return ProcessEffectTextureSet();",
    702: "return ProcessEffectTextureSet();",
    703: "return ProcessEffectTextureSet();",
    674: 'ApplyBottomMusic("effect\\\\curse.bmp", 6); return true;',
    # Existing OnProcess2 implementations, routed straight to their helpers.
    304: 'ApplyTeleportation2("effect\\\\Magic_Violet.tga", 0); return true;',
    307: 'ApplyForestLight("effect\\\\cloud11.tga", 0); return true;',
    308: 'ApplyThrowItem("\\xC0\\xAF\\xC0\\xFA\\xC0\\xCE\\xC5\\xCD\\xC6\\xE4\\xC0\\xCC\\xBD\\xBA\\\\item\\\\\\xB5\\xB9.bmp", 2); return true;',
    309: "ApplyFirstAid(); return true;",
    310: "ApplyParticleUpBurst(); return true;",
    312: 'ApplyHeal("effect\\\\alpha_down.tga", 0); return true;',
    313: 'ApplyHeal2("effect\\\\ring_white.tga", 0); return true;',
    314: 'ApplyExit2("effect\\\\ring_purple.tga"); return true;',
    315: 'return ApplyGlassWall2("effect\\\\alpha_down.tga");',
    316: "ApplyReadyPortal2(); return true;",
    317: "ApplyPortal2(0); return true;",
    318: 'ApplyBottomMagnus("effect\\\\ring_red.tga", 0); return true;',
    319: 'ApplyBottomMagnus("effect\\\\alpha_down.tga", 1); return true;',
    320: 'ApplyHeal("effect\\\\alpha_down.tga", 1); return true;',
    321: 'ApplyWarpZone2("effect\\\\ring_blue.tga"); return true;',
    322: 'ApplyForestLight("effect\\\\cloud11.tga", 1); return true;',
    323: 'ApplyForestLight("effect\\\\cloud11.tga", 2); return true;',
    324: 'ApplyForestLight("effect\\\\cloud11.tga", 10); return true;',
    325: 'ApplyHeal2("effect\\\\ring_white.tga", 1); return true;',
    331: "ApplyHealTime(0); return true;",
    332: "ApplyHealTime(1); return true;",
    334: "ApplyBlind(0); return true;",
    335: "ApplyPoison(0); return true;",
    337: "ApplyJobLvUp50(0); ApplyJobLvUp50(45); return true;",
    338: "ProcessSuperAngelEffect(); return true;",
    339: 'ApplyMagnum2("effect\\\\ring_yellow.tga"); return true;',
    341: "ApplyPortal2(3); return true;",
    342: 'ApplyCoupleCasting("effect\\\\ring_red.tga"); return true;',
    344: "ApplyEntry2(); return true;",
    345: 'ApplySaintWing("effect\\\\wing003.bmp"); return true;',
    346: 'for (short windTime = 0; windTime < 45; windTime = static_cast<short>(windTime + 9)) ApplySphereWind("effect\\\\blue_ivy.bmp", windTime, 0); return true;',
    347: 'for (int paper = 0; paper < 50; ++paper) ApplyColorPaper("effect\\\\white02.bmp"); return true;',
    366: 'for (int lane = 0; lane < 5; ++lane) SpawnBash3D("effect\\\\alpha_center.tga", static_cast<char>(lane), 0); return true;',
    368: "ApplyBodyLight368(); return true;",
    367: "ApplyAuraBladeCasting(); return true;",
    370: 'ApplyBottomMusic("effect\\\\cross_old.bmp", 9); return true;',
    371: "return ProcessEZ2STR();",
    372: "return ProcessEZ2STR();",
    373: "SpawnDragonSmoke(); return true;",
    # Existing OnProcess3 implementations with exact single-helper bodies.
    504: "SpawnChookgi(1); return true;",
    506: "return ProcessSoulParticle506();",
    502: "return ProcessEffect502();",
    505: "return ProcessEffect505();",
    552: "return ProcessSma2Effect552();",
    507: "ProcessEZ2STR(); return true;",
    508: "ProcessEZ2STR(); return true;",
    514: 'ApplyGiExplosion("effect\\\\ring_blue.tga"); return true;',
    515: "SpawnWeatherCloud(); return true;",
    516: "SpawnWeatherCloud(); return true;",
    523: 'ApplyEffectTexture("effect\\\\Tarot01.tga", 1); return true;',
    524: 'ApplyEffectTexture("effect\\\\Tarot02.tga", 1); return true;',
    525: 'ApplyEffectTexture("effect\\\\Tarot03.tga", 1); return true;',
    526: 'ApplyEffectTexture("effect\\\\Tarot04.tga", 1); return true;',
    527: 'ApplyEffectTexture("effect\\\\Tarot05.tga", 1); return true;',
    528: 'ApplyEffectTexture("effect\\\\Tarot06.tga", 1); return true;',
    529: 'ApplyEffectTexture("effect\\\\Tarot07.tga", 1); return true;',
    530: 'ApplyEffectTexture("effect\\\\Tarot08.tga", 1); return true;',
    531: 'ApplyEffectTexture("effect\\\\Tarot09.tga", 1); return true;',
    532: 'ApplyEffectTexture("effect\\\\Tarot10.tga", 1); return true;',
    533: 'ApplyEffectTexture("effect\\\\Tarot11.tga", 1); return true;',
    534: 'ApplyEffectTexture("effect\\\\Tarot12.tga", 1); return true;',
    535: 'ApplyEffectTexture("effect\\\\Tarot13.tga", 1); return true;',
    536: 'ApplyEffectTexture("effect\\\\Tarot14.tga", 1); return true;',
    554: 'ApplyEffectTexture("effect\\\\freeze_a.bmp", 2); return true;',
    581: 'ApplyForestLight("effect\\\\cloud11.tga", 11); return true;',
    582: "ProcessSuperAngelEffect(); return true;",
    570: "return ProcessEffectSprite(0);",
    571: "return ProcessEffectSprite(1);",
    576: "return ProcessEffectSprite(2);",
    577: "return ProcessEffectSprite(3);",
    578: "return ProcessEffectSprite(4);",
    579: "return ProcessEffectSprite(5);",
    580: "return ProcessEffectSprite(6);",
    583: "return ProcessEffectSprite(7);",
    585: "return ProcessEffectSprite(9);",
    586: "return ProcessEffectSprite(10);",
    587: "return ProcessEffectSprite(11);",
    588: "return ProcessEffectSprite(12);",
    589: "return ProcessEffectSprite(13);",
    631: "return ProcessEffectSprite(16);",
    630: "return ProcessEffectSprite(15);",
    632: "return ProcessEffectSprite(17);",
    633: "return ProcessEffectSprite(18);",
    634: "return ProcessEffectSprite(19);",
    637: "return ProcessEffectSprite(20);",
    638: "return ProcessEffectSprite(21);",
    639: "return ProcessEffectSprite(22);",
    640: "return ProcessEffectSprite(23);",
    641: "return ProcessEffectSprite(24);",
    642: "return ProcessEffectSprite(25);",
    643: "return ProcessEffectSprite(26);",
    644: "return ProcessEffectSprite(27);",
    645: "return ProcessEffectSprite(28);",
    647: "return ProcessEffectSprite(29);",
    648: "return ProcessEffectSprite(30);",
    666: "return ProcessEffectSprite(31);",
    682: "return ProcessEffectSprite(32);",
    683: "return ProcessEffectSprite(33);",
    684: "return ProcessEffectSprite(34);",
    685: "return ProcessEffectSprite(35);",
    686: "return ProcessEffectSprite(36);",
    590: "ProcessJobLvUp50_2(); return true;",
    592: "SpawnWeatherCloud(); return true;",
    601: "SpawnSight2(); return true;",
    593: "return ProcessTier3StrEffect();",
    594: "return ProcessTier3StrEffect();",
    595: "return ProcessTier3StrEffect();",
    596: "return ProcessTier3StrEffect();",
    597: "return ProcessTier3StrEffect();",
    598: "return ProcessTier3StrEffect();",
    603: "return ProcessTier3StrEffect();",
    608: "return ProcessTier3StrEffect();",
    609: "return ProcessTier3StrEffect();",
    668: "return ProcessTier3StrEffect();",
    669: "return ProcessTier3StrEffect();",
    677: "return ProcessTier3StrEffect();",
    618: "ApplyKouenka(); return true;",
    619: "ApplyEarthSpike(2); return true;",
    620: 'ApplyBottomVolcano("effect\\\\ring_blue.tga", 14.0f); return true;',
    621: "ApplyStin4(); return true;",
    622: "ApplyThunderStorm2(); return true;",
    624: "ApplyStin5(); return true;",
    627: "ApplyRgCoin3(); return true;",
    628: "ApplyBash3D5(); return true;",
    629: "SpawnChookgi(3); return true;",
    646: "ProcessEZ2STR(); return true;",
    649: "ProcessEZ2STR(); return true;",
    673: "ApplyPoison(2); return true;",
    670: "ProcessEZ2STR(); return true;",
    697: "SpawnWeatherCloud(); return true;",
    698: "SpawnWeatherCloud(); return true;",
    699: "ProcessEZ2STR(); return true;",
}

ACTOR_LIGHT_IDS = {
    414, 415, 417, 419, 420, 421, 422, 423, 424, 425, 426, 427, 458,
    538, 540, 542, 544, 375, 379, 382, 385, 387, 396,
    500, 521, 548, 549, 550, 625, 626, 655, 656, 667,
}
BLIND_IDS = set(range(475, 485))

DIRECT_GROUP_CALLS = {
    442: "ApplyBodyLight442(); return true;",
    446: "ApplyBodyLight446(); return true;",
    449: "ApplyBodyLight449(); return true;",
    172: 'ApplyElementalChangeCasting(); return true;',
    173: 'ApplyElementalChangeCasting(); return true;',
    174: 'ApplyElementalChangeCasting(); return true;',
    175: 'ApplyElementalChangeCasting(); return true;',
    176: 'ApplyElementalChangeCasting(); return true;',
    177: 'ApplyElementalChangeCasting(); return true;',
    178: 'ApplyElementalChangeCasting(); return true;',
    179: 'ApplyElementalChangeCasting(); return true;',
    222: 'ApplyDefender("effect\\\\ring_black.tga"); return true;',
    231: 'ApplyMapPillar("effect\\\\ring_blue.tga", 4.0f, false); return true;',
    247: 'ApplyMapPillar("effect\\\\ring_blue.tga", 4.0f, true); return true;',
    252: 'ApplyDefender("effect\\\\ring_yellow.tga"); return true;',
    259: 'ApplyMapPillar("effect\\\\magic_green.tga", 7.0f, false); return true;',
    260: 'ApplyMapPillar("effect\\\\ring_red.tga", 6.0f, false); return true;',
    280: 'ApplyBottomLight("effect\\\\twirl.bmp", 8); return true;',
    283: 'ApplyBottomOut("effect\\\\safeline.bmp", 0); return true;',
    285: 'ApplyBottomLight("effect\\\\twirl.bmp", 4); return true;',
    223: 'ApplyVolcano("effect\\\\ring_white.tga", 2, false); return true;',
    225: 'ApplyVolcano("effect\\\\ring_red.tga", 0, false); return true;',
    236: 'ApplyVolcano("effect\\\\ring_blue.tga", 0, false); return true;',
    237: 'ApplyVolcano("effect\\\\magic_green.tga", 0, false); return true;',
    238: 'ApplyVolcano("effect\\\\ring_white.tga", 0, false); return true;',
    261: 'ApplyVolcano("effect\\\\ring_yellow.tga", 0, true); return true;',
    263: 'ApplyVolcano("effect\\\\ring_yellow.tga", 1, false); return true;',
    239: 'ApplyBottomVolcano("effect\\\\ring_red.tga", 0.0f); return true;',
    240: 'ApplyBottomVolcano("effect\\\\ring_blue.tga", 0.0f); return true;',
    241: 'ApplyBottomVolcano("effect\\\\magic_green.tga", 0.0f); return true;',
    620: 'ApplyBottomVolcano("effect\\\\ring_blue.tga", 14.0f); return true;',
    224: 'ApplyWind(); return true;',
    436: 'ApplyHalfSphere("effect\\\\white02.bmp"); return true;',
    437: 'ApplyHalfSphere("effect\\\\white02.bmp"); ApplyAttackEnergy("effect\\\\ring_blue.tga"); return true;',
    433: 'ApplyElectric2(); ApplyElectric2(); return true;',
}

INIT_HANDLER_CALLS = {
    20: "SpawnEnchantPoison", 22: "SpawnSight", 24: "SpawnFireBall",
    33: "SpawnRuwach", 42: "SpawnBlessing", 44: "SpawnSmoke", 47: "SpawnTorch",
    125: "SpawnEnchantPoison",
    229: "SpawnWeatherCloud", 230: "SpawnWeatherCloud", 233: "SpawnWeatherCloud",
    515: "SpawnWeatherCloud", 516: "SpawnWeatherCloud", 592: "SpawnWeatherCloud",
    601: "SpawnSight2", 697: "SpawnWeatherCloud", 698: "SpawnWeatherCloud",
    493: "ProcessEnchantPoison2",
}

def load_rows() -> tuple[list[dict[str, str]], dict[int, dict[str, str]]]:
    with INDEX.open(encoding="utf-8", newline="") as stream:
        index = list(csv.DictReader(stream, delimiter="\t"))
    with COVERAGE.open(encoding="utf-8", newline="") as stream:
        coverage = {int(row["id"]): row for row in csv.DictReader(stream, delimiter="\t")}
    if len(index) != 693 or len(coverage) != 693:
        raise SystemExit("expected exactly 693 index and coverage rows")
    return index, coverage


def escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def route(row: dict[str, str], coverage: dict[str, str], tier: str) -> str:
    effect_id = int(row["case"])
    status = coverage["status"]
    handler = row["handler"]
    if effect_id in FORCE_UNPORTED:
        return "return UnportedEffect();"
    if effect_id in DIRECT_CALLS and (status in {"ported", "routed"} or effect_id in FORCE_ROUTE):
        return DIRECT_CALLS[effect_id]
    if effect_id in ACTOR_LIGHT_IDS and (status in {"ported", "routed"} or effect_id in FORCE_ROUTE):
        return "ApplyActorLight(); return true;"
    if effect_id in BLIND_IDS and (status in {"ported", "routed"} or effect_id in FORCE_ROUTE):
        return "ApplyBlind(m_type - 474); return true;"
    if effect_id in DIRECT_GROUP_CALLS and (status in {"ported", "routed"} or effect_id in FORCE_ROUTE):
        return DIRECT_GROUP_CALLS[effect_id]
    if status not in {"ported", "routed"}:
        return "return UnportedEffect();"
    if effect_id == 185:
        return "ProcessTargetPositionStr(); return FinishEffectFrame();"
    if tier == "OnProcess2":
        if effect_id in {305, 306, 311, 369}:
            return "return OnProcess2Legacy();"
        if effect_id == 366:
            return 'for (int lane = 0; lane < 5; ++lane) SpawnBash3D("effect\\\\alpha_center.tga", static_cast<char>(lane), 0); return true;'
        if "ProcessEZ2STR" in handler:
            return "return OnProcess2Legacy();"
    if "ProcessEZ2STR" in handler:
        return "return ProcessEZ2STR();"
    if effect_id in INIT_HANDLER_CALLS:
        method = INIT_HANDLER_CALLS[effect_id]
        if method == "ProcessEnchantPoison2":
            return f"{method}(); return true;"
        return f"{method}(); return true;"
    if tier == "OnProcess2":
        return "return OnProcess2Legacy();"
    return "return ProcessEffectFrameLegacy();"


def write_switch(path: Path, rows: list[dict[str, str]], tier: str,
                 coverage: dict[int, dict[str, str]]) -> None:
    output = ["// Generated by tools/generate_effect_dispatch.py; do not edit by hand."]
    for row in rows:
        effect_id = int(row["case"])
        output.append(f"case {effect_id}: {route(row, coverage[effect_id], tier)}")
    path.write_text("\n".join(output) + "\n", encoding="utf-8", newline="\n")


def main() -> None:
    index, coverage = load_rows()
    for tier, filename in (
        ("OnProcess", "RagEffectOnProcessCases.generated.inc"),
        ("OnProcess2", "RagEffectOnProcess2Cases.generated.inc"),
        ("OnProcess3", "RagEffectOnProcess3Cases.generated.inc"),
    ):
        rows = [row for row in index if row["tier"] == tier]
        write_switch(OUT / filename, rows, tier, coverage)

    names = ["// Generated by tools/generate_effect_dispatch.py; do not edit by hand."]
    names.extend(f'case {int(row["case"])}: return "{escape(row["handler"])}";' for row in index)
    (OUT / "RagEffectHandlerNameCases.generated.inc").write_text(
        "\n".join(names) + "\n", encoding="utf-8", newline="\n")
    print("generated 416 / 73 / 204 dispatcher cases and 693 Ref handler names")


if __name__ == "__main__":
    main()
