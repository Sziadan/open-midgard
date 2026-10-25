void ApplyUseSkillEffects(CGameMode& mode, CGameActor* source, CGameActor* target, int skillId, int rawLevel, u32 sourceGid, u32 targetGid)
{
    int clampedLevel = 0;
    int i = 0;
    int v13 = 0;
    int v14 = 0;
    int v18 = 0;
    int v48 = 0;
    int v49 = 0;
    int v50 = 0;
    int v51 = 0;
    int v52 = 0;
    int v53 = 0;
    int v54 = 0;
    int v57 = 0;
    CAbleToMakeEffect* v15 = nullptr;
    CAbleToMakeEffect* v17 = nullptr;
    CRagEffect* v29 = nullptr;
    CRagEffect* v43 = nullptr;
    CRagEffect* v47 = nullptr;
    CRagEffect* v34 = nullptr;
    CRagEffect* v37 = nullptr;
    CRagEffect* v39 = nullptr;
    CRagEffect* v31 = nullptr;
    CRagEffect* v20 = nullptr;
    CRagEffect* v23 = nullptr;
    CRagEffect* v25 = nullptr;
    CRagEffect* v26 = nullptr;
    CRagEffect* v32 = nullptr;
    CMsgEffect* m_birdEffect = nullptr;

        {
            v13 = rawLevel;
            clampedLevel = v13;
            if ( v13 >= 1 )
            {
                if ( v13 > 10 && skillId != 9 )
                    clampedLevel = 10;
            }
            else
            {
                clampedLevel = 1;
            }
            v14 = skillId;
            if ( (unsigned short)v14 > 0x157u )
            {
                if ( skillId > 0x1E6u )
                {
                    if ( skillId > 0x218u )
                    {
                        if ( skillId > 0x3F5u )
                        {
                            if ( skillId > 0x1F48u )
                            {
                                switch ( skillId )
                                {
                                    case 0x1F4A:
                                    case 0x1F4B:
                                        if (source != nullptr)
                                        {
                                            LaunchOn(source, 564);
                                        }
                                        break;
                                    case 0x1F50:
                                        if (source != nullptr)
                                        {
    LABEL_159:
                                            LaunchOn(source, 183);
                                        }
                                        break;
                                    case 0x200C:
    L_159054:
                                        if (source != nullptr)
                                            goto LABEL_494;
                                        break;
                                    case 0x200E:
    L_159108:
                                        if (source != nullptr)
                                        {
                                            if (source != nullptr) {
                                                source->SendMsg(source, 109, 368, 0, 0);
                                            }
                                            LaunchOn(source, 368);
                                            mode.SendMsg(135, 0, 0, 0);
                                            PlayUseSkillWave("_stonecurse.wav", 0.0, 0.0, 0.0);
                                        }
                                        break;
                                    case 0x201B:
    L_159068:
                                        if (source != nullptr)
                                        {
                                            LaunchOn(source, 222);
                                        }
                                        break;
                                    case 0x201C:
    L_159049:
                                        if (source != nullptr)
    LABEL_494:
                                            if (source != nullptr) {
                                                source->SendMsg(source, 127, 0, 0, 0);
                                            }
                                        break;
                                    case 0x201E:
    L_158817:
                                        if (source != nullptr)
                                        {
                                            LaunchOn(source, 76);
                                        }
                                        break;
                                        return;
                                }
                            }
                            else if ( v14 == 8008 )
                            {
                                if (source != nullptr)
                                {
                                    LaunchOn(source, 571);
                                }
                            }
                            else if ( skillId > 0x1F42u )
                            {
                                v53 = v14 - 8004;
                                if ( v53 )
                                {
                                    v54 = v53 - 1;
                                    if ( v54 )
                                    {
                                        if ( v54 == 1 )
                                        {
                                            PlayUseSkillWave("effect\\mon_\xC6\xF8\xB1\xE2.wav", 0.0, 0.0, 0.0);
                                            if (source != nullptr)
                                            {
                                                LaunchOn(source, 569);
                                            }
                                            if (target != nullptr)
                                            {
                                                LaunchOn(target, 569);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        PlayUseSkillWave("effect\\mon_\xB8\xCD\xB7\xE6\xB0\xFA\xB0\xAD.wav", 0.0, 0.0, 0.0);
                                        if (source != nullptr)
                                        {
                                            LaunchOn(source, 570);
                                        }
                                        if (target != nullptr)
                                        {
                                            LaunchOn(target, 570);
                                        }
                                    }
                                }
                                else if (source != nullptr)
                                {
    LABEL_286:
                                    LaunchOn(source, 505);
                                }
                            }
                            else if ( v14 == 8002 )
                            {
                                if (source != nullptr)
                                {
                                    LaunchOn(source, 456);
                                }
                                if (target != nullptr)
                                {
                                    LaunchOn(target, 456);
                                }
                            }
                            else if ( skillId > 0x3FBu )
                            {
                                if ( v14 == 8001 )
                                {
    L_158937_0:
                                    if (target != nullptr)
                                    {
                                        v52 = static_cast<unsigned short>(rawLevel);
                                        if ( v52 < 4000 )
                                        {
                                            if ( v52 < 2000 )
                                            {
                                                if ( v52 < 200 )
                                                    v57 = 7;
                                                else
                                                    v57 = 312;
                                                LaunchOn(target, v57);
                                            }
                                            else
                                            {
                                                LaunchOn(target, 313);
                                            }
                                        }
                                        else
                                        {
                                            LaunchOn(target, 325);
                                        }
                                        if (target != nullptr) {
                                            target->SendMsg(target, 88, rawLevel, -16711936, 114);
                                        }
                                    }
                                }
                            }
                            else if ( v14 == 1019 )
                            {
    L_159043:
                                if (target != nullptr)
                                {
                                    LaunchOn(target, 257);
                                }
                            }
                            else
                            {
                                v51 = v14 - 1017;
                                if ( v51 )
                                {
                                    if ( v51 == 1 )
                                    {
    L_159037:
                                        if (target != nullptr)
                                        {
                                            LaunchOn(target, 255);
                                        }
                                    }
                                }
                                else
                                {
    L_159046_0:
                                    if (target != nullptr)
                                    {
                                        LaunchOn(target, 258);
                                    }
                                }
                            }
                        }
                        else if ( v14 == 1013 )
                        {
                            if (source != nullptr)
                                PlayUseSkillWave("effect\\\xBC\xAE\xBE\xE7\xC0\xC7 \xBE\xEE\xBD\xD8\xBD\xC5.wav", 0.0, 0.0, 0.0);
                        }
                        else if ( skillId > 0x2B4u )
                        {
                            if ( skillId > 0x3EDu )
                            {
                                v49 = v14 - 1008;
                                if ( v49 )
                                {
                                    v50 = v49 - 2;
                                    if ( v50 )
                                    {
                                        if ( v50 == 1 )
                                        {
                                            if (source != nullptr)
                                            {
                                                if (target != nullptr)
                                                {
                                                    if (source != nullptr) {
                                                        source->SendMsg(source, 21, 30, 0, 0);
                                                    }
                                                    PlayUseSkillWave("effect\\\xBA\xEA\xB6\xF3\xB1\xE2\xC0\xC7 \xBD\xC3.wav", 0.0, 0.0, 0.0);
                                                    v29 = LaunchOn(source, 607);
                                                    if ( v29 )
                                                    {
    LABEL_252:
                                                        if (v29 != nullptr && target != nullptr) {
                                                            v29->SendMsg(v29, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else if (source != nullptr)
                                    {
                                        if (target != nullptr)
                                        {
                                            source->SetForceAnimation(96, 4, 5, 96, 4, 1, 0x708, 1);
                                            PlayUseSkillWave("effect\\\xC0\xCC\xB5\xD0\xC0\xC7 \xBB\xE7\xB0\xFA.wav", 0.0, 0.0, 0.0);
                                            v43 = LaunchOn(source, 606);
                                            if ( v43 )
                                            {
    LABEL_379:
                                                if (v43 != nullptr && target != nullptr) {
                                                    v43->SendMsg(v43, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
                                                }
                                            }
                                        }
                                    }
                                }
                                else
                                {
    L_159040:
                                    if (target != nullptr)
                                    {
                                        LaunchOn(target, 256);
                                    }
                                }
                            }
                            else if ( v14 == 1005 )
                            {
                                if (target != nullptr)
                                {
                                    LaunchOn(target, 602);
                                }
                            }
                            else if ( skillId > 0x2B7u )
                            {
                                if ( v14 == 1002 && source )
                                {
                                    LaunchOn(source, 599);
                                }
                            }
                            else if ( v14 == 695 )
                            {
                                if (source != nullptr)
                                    PlayUseSkillWave("effect\\\xC8\xEF\xBE\xF3\xB0\xC5\xB8\xB2.wav", 0.0, 0.0, 0.0);
                            }
                            else
                            {
                                v48 = v14 - 693;
                                if ( v48 )
                                {
                                    if ( v48 == 1 && target != nullptr)
                                    {
                                        LaunchOn(target, 91);
                                    }
                                }
                                else if (target != nullptr)
                                {
                                    LaunchOn(target, 699);
                                }
                            }
                        }
                        else if ( v14 == 692 )
                        {
                            if (source != nullptr)
                                PlayUseSkillWave("effect\\\xB3\xAA\xB8\xA6\xC0\xD8\xC1\xF6\xB8\xBB\xBE\xC6\xBF\xE4.wav", 0.0, 0.0, 0.0);
                        }
                        else
                        {
                            switch ( skillId )
                            {
                                case 0x21B:
                                    if (source != nullptr)
                                    {
                                        LaunchOn(source, 636);
                                    }
                                    break;
                                case 0x21D:
                                    if (source != nullptr)
                                    {
                                        LaunchOn(source, 622);
                                    }
                                    break;
                                case 0x22B:
                                    if (source != nullptr)
                                        if (source != nullptr) {
                                            source->SendMsg(source, 63, 0, 0, 0);
                                        }
                                    if (target != nullptr)
                                    {
                                        if (target != nullptr) {
                                            target->SendMsg(target, 63, 0, 0, 0);
                                        }
                                        LaunchOn(target, 344);
                                    }
                                    break;
                                case 0x23C:
                                case 0x23D:
                                case 0x23E:
                                case 0x23F:
    L_159405:
                                    if (target != nullptr)
                                    {
                                        v47 = LaunchOn(target, 503);
                                        if ( v47 )
                                            if (v47 != nullptr && target != nullptr) {
                                                v47->SendMsg(v47, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
                                            }
                                        if (target != nullptr) {
                                            target->SendMsg(target, 109, 424, 0, 0);
                                        }
                                        LaunchOn(target, 424);
                                    }
                                    break;
                                case 0x242:
                                    if (source != nullptr)
                                    {
                                        LaunchOn(source, 220);
                                    }
                                    break;
                                case 0x2B1:
    L_158965:
                                    if (target != nullptr)
                                    {
                                        LaunchOn(target, 42);
                                    }
                                    break;
                                case 0x2B2:
    L_158949:
                                    if (target != nullptr)
                                    {
                                        LaunchOn(target, 37);
                                    }
                                    break;
                                case 0x2B3:
    L_159241:
                                    if (target != nullptr)
                                    {
                                        LaunchOn(target, 375);
                                        LaunchOn(target, 440);
                                    }
                                    break;
                                    return;
                            }
                        }
                    }
                    else if ( v14 == 536 )
                    {
                        if (target != nullptr)
                        {
                            LaunchOn(target, 635);
                        }
                    }
                    else
                    {
                        switch ( skillId )
                        {
                            case 0x1E7:
                                if (source != nullptr)
                                {
                                    LaunchOn(source, 500);
                                }
                                PlayUseSkillWave("effect\\\xB4\xE7\xBD\xC5\xC0\xBB \xC0\xA7\xC7\xD1 \xBC\xAD\xBA\xF1\xBD\xBA.wav", 0.0, 0.0, 0.0);
                                return;
                            case 0x1E8:
                                PlayUseSkillWave("effect\\\xC0\xDA\xC0\xE5\xB0\xA1.wav", 0.0, 0.0, 0.0);
                                return;
                            case 0x1E9:
                                if (target != nullptr)
                                {
                                    PlayUseSkillWave("effect\\\xBF\xB5\xBF\xF8\xC0\xC7 \xC8\xA5\xB5\xB7.wav", 0.0, 0.0, 0.0);
                                    LaunchOn(target, 500);
                                }
                                return;
                            case 0x1ED:
                                PlayUseSkillWave("effect\\\xC0\xFC\xC0\xE5\xC0\xC7.wav", 0.0, -100.0, 0.0);
                                return;
                            case 0x1EE:
                                goto L_159405;
                            case 0x1EF:
                                if (source != nullptr)
                                {
    LABEL_81:
                                    if (source != nullptr) {
                                        source->SendMsg(source, 109, 130, 0, 0);
                                    }
                                    LaunchOn(source, 130);
                                }
                                return;
                            case 0x1F4:
                                PlayUseSkillWave("effect\\\xB4\xCF\xBA\xA7\xB7\xEE\xB0\xD5\xC0\xC7 \xB9\xDD\xC1\xF6.wav", 0.0, 0.0, 0.0);
                                return;
                            case 0x1F8:
                                if (source != nullptr)
                                {
                                    LaunchOn(source, 625);
                                }
                                return;
                            case 0x1F9:
                                if (source != nullptr)
                                    goto LABEL_374;
                                return;
                            case 0x1FA:
                                if (source != nullptr)
                                {
    LABEL_300:
                                    LaunchOn(source, 456);
                                }
                                return;
                            case 0x1FC:
                                PlayUseSkillWave("effect\\\xBD\xC9\xBF\xAC\xBC\xD3\xC0\xB8\xB7\xCE.wav", 0.0, 0.0, 0.0);
                                return;
                            case 0x201:
                                if (target != nullptr)
                                {
                                    v43 = LaunchOn(target, 627);
                                    if ( v43 )
                                        goto LABEL_379;
                                }
                                return;
                            case 0x204:
                                if (source != nullptr)
                                {
                                    LaunchOn(source, 637);
                                }
                                return;
                            case 0x205:
                                if (source != nullptr)
                                {
    LABEL_374:
                                    LaunchOn(source, 626);
                                }
                                break;
                            case 0x20F:
                                PlayUseSkillWave("effect\\\xBA\xD2\xBB\xE7\xBD\xC5.wav", 0.0, 0.0, 0.0);
                                break;
                                return;
                        }
                    }
                }
                else if ( v14 == 486 )
                {
    L_158836:
                    if (source != nullptr)
                    {
                        if (source != nullptr) {
                            source->SendMsg(source, 109, 128, 0, 0);
                        }
                        LaunchOn(source, 128);
                    }
                }
                else
                {
                    switch ( skillId )
                    {
                        case 0x15D:
                            if (source != nullptr)
                            {
                                PlayUseSkillWave("effect\\\xB1\xE8\xBC\xAD\xB9\xE6\xB5\xB7.wav", 0.0, 0.0, 0.0);
                                LaunchOn(source, 455);
                            }
                            return;
                        case 0x15E:
                            if (source != nullptr)
                                goto LABEL_300;
                            return;
                        case 0x163:
                            if (source != nullptr)
                            {
                                if (source != nullptr) {
                                    source->SendMsg(source, 109, 367, 0, 0);
                                }
                                LaunchOn(source, 367);
                                if (source != nullptr) {
                                    source->SendMsg(source, 109, 474, 0, 0);
                                }
                                LaunchOn(source, 474);
                                source->SetForceAnimation(32, 0, 5, 32, 5, 1, 0x3E8, 0);
                                PlayUseSkillWave("effect\\\xB7\xCE\xC5\xB0.wav", 0.0, 0.0, 0.0);
                            }
                            return;
                        case 0x164:
                            goto L_159054;
                        case 0x165:
                            if (source != nullptr)
                            {
                                if (source != nullptr) {
                                    source->SendMsg(source, 109, 369, 0, 0);
                                }
                                LaunchOn(source, 369);
                            }
                            return;
                        case 0x167:
                        case 0x168:
                            goto L_159108;
                        case 0x169:
                            goto L_159241;
                        case 0x16C:
                            if (source != nullptr && target != nullptr)
                            {
                                source->SetForceAnimation(88, 0, 3, 88, 3, 1, 0x3E8, 0);
                                v34 = LaunchOn(source, 378);
                                if ( v34 )
                                {
                                    if (v34 != nullptr && target != nullptr) {
                                        v34->SendMsg(v34, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
                                    }
                                }
                                LaunchOn(target, 379);
                            }
                            return;
                        case 0x16E:
                            if (target == nullptr)
                                return;
                            if (target != nullptr) {
                                target->SendMsg(target, 109, 382, 0, 0);
                            }
                            LaunchOn(target, 382);
                            break;
                        case 0x175:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 383);
                            }
                            return;
                        case 0x176:
                            if (source != nullptr && target != nullptr)
                            {
                                v37 = LaunchOn(source, 384);
                                if ( v37 )
                                    if (v37 != nullptr && target != nullptr) {
                                        v37->SendMsg(v37, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
                                    }
                                LaunchOn(source, 385);
                                LaunchOn(target, 385);
                                LaunchOn(source, 407);
                                LaunchOn(target, 407);
                            }
                            return;
                        case 0x177:
                            if (target != nullptr)
                            {
                                LaunchOn(target, 406);
                                LaunchOn(target, 380);
                            }
                            return;
                        case 0x17A:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 493);
                            }
                            return;
                        case 0x17C:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 386);
                            }
                            return;
                        case 0x17F:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 389);
                            }
                            return;
                        case 0x180:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 390);
                            }
                            return;
                        case 0x183:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 391);
                            }
                            return;
                        case 0x185:
                            if (source != nullptr)
                                if (source != nullptr) {
                                    source->SendMsg(source, 63, 0, 0, 0);
                                }
                            return;
                        case 0x186:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 392);
                            }
                            return;
                        case 0x18B:
                            if ( source && !EffectExists(source, 394) )
                            {
                                LaunchOn(source, 394);
                            }
                            PlayUseSkillWave("effect\\\xC7\xE0\xBF\xEE\xC0\xC7.wav", 0.0, 0.0, 0.0);
                            return;
                        case 0x18C:
                            if (source != nullptr && target != nullptr)
                            {
                                v39 = LaunchOn(source, 395);
                                if ( v39 )
                                    if (v39 != nullptr && target != nullptr) {
                                        v39->SendMsg(v39, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
                                    }
                                LaunchOn(source, 396);
                                LaunchOn(target, 396);
                            }
                            return;
                        case 0x192:
                            if (target != nullptr)
                            {
                                LaunchOn(target, 403);
                            }
                            return;
                        case 0x193:
                            if (source != nullptr)
                                goto LABEL_286;
                            return;
                        case 0x196:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 409);
                            }
                            return;
                        case 0x198:
                            if (target != nullptr)
                            {
                                LaunchOn(target, 408);
                            }
                            return;
                        case 0x19B:
                            if (source != nullptr)
                            {
                                if ( EffectExists(source, 443) )
                                {
                                    LaunchOn(source, 444);
                                }
                                else if ( !EffectExists(source, 442) )
                                {
                                    LaunchOn(source, 442);
                                }
                            }
                            return;
                        case 0x19D:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 435);
                            }
                            return;
                        case 0x1A4:
                            PlayUseSkillWave("effect\\\xBF\xC0\xB6\xF3 \xBA\xED\xB7\xB9\xC0\xCC\xB5\xE5.wav", 0.0, -150.0, 0.0);
                            return;
                        case 0x1A9:
                            if (source != nullptr)
                            {
                                PlayUseSkillWave("effect\\mon_\xC6\xF8\xB1\xE2.wav", 0.0, 0.0, 0.0);
                                while ( EffectExists(source, 461) )
                                    if (source != nullptr) {
                                        source->SendMsg(source, 109, 461, 0, 0);
                                    }
                                LaunchOn(source, 461);
                                i = clampedLevel + 466;
                                while (EffectExists(source, i)) {
                                    source->SendMsg(source, 109, i, 0, 0);
                                }
                                LaunchOn(source, i);
                            }
                            return;
                        case 0x1AA:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 446);
                                while ( EffectExists(source, 445) )
                                    if (source != nullptr) {
                                        source->SendMsg(source, 109, 445, 0, 0);
                                    }
                            }
                            return;
                        case 0x1AC:
                        case 0x1AD:
                        case 0x1AE:
                            if (source != nullptr && target != nullptr)
                            {
                                if (source != nullptr) {
                                    source->SendMsg(source, 109, 418, 0, 0);
                                }
                                LaunchOn(source, 418);
                                if (source != nullptr) {
                                    source->SendMsg(source, 109, 544, 0, 0);
                                }
                                LaunchOn(source, 544);
                                LaunchOn(source, 572);
                            }
                            return;
                        case 0x1AF:
                        case 0x1B0:
                        case 0x1B1:
                            if (source != nullptr)
                            {
                                if (source != nullptr) {
                                    source->SendMsg(source, 109, 488, 0, 0);
                                }
                                LaunchOn(source, 488);
                                LaunchOn(source, 543);
                            }
                            return;
                        case 0x1B2:
                            if (target != nullptr)
                            {
                                PlayUseSkillWave("effect\\\xB8\xB6\xB9\xFD\xB7\xC2 \xC1\xF5\xC6\xF8.wav", 0.0, 0.0, 0.0);
                                if (target != nullptr) {
                                    target->SendMsg(target, 109, 543, 0, 0);
                                }
                                LaunchOn(target, 543);
                            }
                            return;
                        case 0x1BC:
                            if (source != nullptr)
                            {
                                PlayUseSkillWave("effect\\\xB4\xDE\xBA\xFB.wav", 0.0, 0.0, 0.0);
                                mode.SendMsg(135, 0, 0, 0);
                            }
                            return;
                        case 0x1BD:
                        case 0x1BF:
                        case 0x1C0:
                        case 0x1C1:
                        case 0x1C2:
                        case 0x1C3:
                        case 0x1C4:
                        case 0x1C5:
                        case 0x1C6:
                        case 0x1C7:
                        case 0x1C8:
                        case 0x1C9:
                        case 0x1CA:
                        case 0x1CC:
                        case 0x1CD:
                            goto L_159405;
                        case 0x1BE:
                            if (source != nullptr && target != nullptr)
                            {
                                v31 = LaunchOn(source, 541);
                                v32 = v31;
                                if ( v31 )
                                {
                                    if (v31 != nullptr && target != nullptr) {
                                        v31->SendMsg(v31, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
                                    }
                                    if (v32 != nullptr) {
                                        v32->SendMsg(v32, 44, 4, 0, 0);
                                    }
                                }
                                LaunchOn(target, 220);
                            }
                            return;
                        case 0x1CB:
    L_158827:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 98);
                            }
                            return;
                        case 0x1CE:
                            if (target != nullptr)
                            {
                                PlayUseSkillWave("effect\\T_\xB3\xAB\xB9\xFD.wav", 0.0, 0.0, 0.0);
                                LaunchOn(target, 543);
                                LaunchOn(target, 590);
                            }
                            return;
                        case 0x1CF:
                            if (target != nullptr)
                            {
                                PlayUseSkillWave("effect\\T_\xB9\xD9\xB6\xF7\xB9\xE6\xC3\xE2.wav", 0.0, 0.0, 0.0);
                                LaunchOn(target, 543);
                            }
                            return;
                        case 0x1D0:
                            if (target != nullptr)
                            {
                                PlayUseSkillWave("effect\\T_\xBA\xAF\xBD\xC5.wav", 0.0, 0.0, 0.0);
                                LaunchOn(target, 542);
                            }
                            return;
                        case 0x1D1:
                            if (target != nullptr)
                            {
                                PlayUseSkillWave("effect\\T_\xB5\xEE\xB7\xCF.wav", 0.0, 0.0, 0.0);
                                if (target != nullptr) {
                                    target->SendMsg(target, 109, 419, 0, 0);
                                }
                                LaunchOn(target, 419);
                                LaunchOn(target, 542);
                            }
                            return;
                        case 0x1D6:
                            if (target != nullptr)
                            {
                                PlayUseSkillWave("effect\\T_\xBD\xB4\xBF\xF4.wav", 0.0, -50.0, 0.0);
                                if (target != nullptr) {
                                    target->SendMsg(target, 109, 420, 0, 0);
                                }
                                LaunchOn(target, 420);
                                LaunchOn(target, 589);
                            }
                            return;
                        case 0x1D7:
                            if (target != nullptr)
                            {
                                PlayUseSkillWave("effect\\T_\xB0\xF8\xB0\xDD\xB7\xC2.wav", 0.0, 0.0, 0.0);
                                if (target != nullptr) {
                                    target->SendMsg(target, 109, 427, 0, 0);
                                }
                                LaunchOn(target, 427);
                            }
                            return;
                        case 0x1D8:
                            if (target != nullptr)
                            {
                                PlayUseSkillWave("effect\\T_\xB9\xE6\xBE\xEE\xC7\xFC.wav", 0.0, 0.0, 0.0);
                                mode.SendMsg(135, 0, 0, 0);
                                if (target != nullptr) {
                                    target->SendMsg(target, 109, 254, 0, 0);
                                }
                                LaunchOn(target, 254);
                                LaunchOn(target, 261);
                            }
                            return;
                        case 0x1DB:
                            if (source != nullptr)
                            {
                                LaunchOn(source, 496);
                            }
                            return;
                        case 0x1DC:
                            if (target != nullptr)
                            {
                                v29 = LaunchOn(target, 495);
                                if ( v29 )
                                    goto LABEL_252;
                            }
                            return;
                        case 0x1DF:
                            if (target != nullptr)
                            {
                                LaunchOn(target, 300);
                                LaunchOn(target, 500);
                            }
                            return;
                        case 0x1E2:
                            if (target == nullptr)
                                return;
                            LaunchOn(target, 521);
                            break;
                            return;
                    }
                    PlayUseSkillWave("effect\\priest_resurrection.wav", 0.0, 0.0, 0.0);
                }
            }
            else if ( (unsigned short)v14 == 343 )
            {
    L_159137_0:
                if (target != nullptr)
                {
                    LaunchOn(target, 269);
                }
            }
            else
            {
                switch ( skillId )
                {
                    case 6:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 67);
                        }
                        return;
                    case 7:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 17);
                        }
                        return;
                    case 8:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 11);
                        }
                        return;
                    case 9:
                        if (target != nullptr)
                            if (target != nullptr) {
                                target->SendMsg(target, 88, clampedLevel, -16776961, 114);
                            }
                        return;
                    case 0x10:
                        if ( target && v14 == 16 )
                        {
                            LaunchOn(target, 23);
                            PlayUseSkillWaveAt(target != nullptr ? target->m_pos : vector3d{}, "effect\\T_\xBA\xB8\xC1\xB6\xB8\xB6\xB9\xFD.wav");
                        }
                        return;
                    case 0x1C:
                    case 0x14E:
                        goto L_158937_0;
                    case 0x1D:
                        goto L_158949;
                    case 0x1E:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 38);
                        }
                        return;
                    case 0x1F:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 39);
                        }
                        return;
                    case 0x20:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 40);
                        }
                        return;
                    case 0x21:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 41);
                        }
                        return;
                    case 0x22:
                        goto L_158965;
                    case 0x23:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 66);
                        }
                        return;
                    case 0x2D:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 153);
                        }
                        return;
                    case 0x32:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 18);
                        }
                        return;
                    case 0x35:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 21);
                        }
                        return;
                    case 0x36:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 77);
                            v18 = target != nullptr ? target->m_job : 0;
                            if ( (v18 > 4000 && v18 < 6000 || static_cast<unsigned>(v18) < 28u) )
                            {
                                LaunchOn(target, 140);
                            }
                        }
                        return;
                    case 0x3C:
                        if (source != nullptr)
                            goto LABEL_81;
                        return;
                    case 0x3D:
                        if (source != nullptr)
                        {
                            if (source != nullptr) {
                                source->SendMsg(source, 82, 400 * rawLevel, 0, 0);
                            }
                            source->SetState(13);
                            if ( sourceGid != g_session.m_aid )
                                if (source != nullptr) {
                                    source->SendMsg(source, 83, 0, 0, 0);
                                }
                        }
                        return;
                    case 0x42:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 84);
                        }
                        return;
                    case 0x43:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 88);
                        }
                        return;
                    case 0x44:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 86);
                        }
                        return;
                    case 0x47:
                        if ( g_serviceType == ServiceKorea && target != nullptr)
                        {
                            LaunchOn(target, 136);
                        }
                        return;
                    case 0x48:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 78);
                        }
                        return;
                    case 0x49:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 112);
                        }
                        return;
                    case 0x4A:
                        goto L_158817;
                    case 0x4B:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 75);
                        }
                        return;
                    case 0x4C:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 87);
                        }
                        return;
                    case 0x4E:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 85);
                        }
                        return;
                    case 0x51:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 62);
                        }
                        return;
                    case 0x58:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 28);
                        }
                        return;
                    case 0x6C:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 101);
                        }
                        return;
                    case 0x6F:
                        goto L_158827;
                    case 0x70:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 103);
                        }
                        return;
                    case 0x71:
                        goto L_158836;
                    case 0x72:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 104);
                        }
                        return;
                    case 0x7C:
                        v17 = mode.m_world != nullptr ? mode.m_world->GetSkillByAID(targetGid) : nullptr;
                        if ( v17 )
                        {
                            LaunchOn(v17, 100);
                        }
                        return;
                    case 0x83:
                        if (source != nullptr)
                        {
                            v15 = mode.m_world != nullptr ? mode.m_world->GetSkillByAID(targetGid) : nullptr;
                            if ( v15 )
                            {
                                LaunchOn(v15, 111);
                                m_birdEffect = source->m_birdEffect;
                                if ( m_birdEffect )
                                    if (m_birdEffect != nullptr) {
                                        m_birdEffect->SendMsg(m_birdEffect, 29, reinterpret_cast<msgparam_t>(&v15->m_pos), 0, 0);
                                    }
                            }
                        }
                        return;
                    case 0x8A:
                        if (target != nullptr)
                            goto LABEL_75;
                        return;
                    case 0x8B:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 126);
                        }
                        return;
                    case 0x8D:
                        if (target != nullptr)
                        {
    LABEL_75:
                            LaunchOn(target, 125);
                        }
                        break;
                    case 0x8E:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 309);
                        }
                        break;
                    case 0x9B:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 311);
                        }
                        break;
                    case 0x9D:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 169);
                        }
                        break;
                    case 0xA2:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 174);
                        }
                        break;
                    case 0xA3:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 177);
                        }
                        break;
                    case 0xA4:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 173);
                        }
                        break;
                    case 0xA5:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 175);
                        }
                        break;
                    case 0xA6:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 179);
                        }
                        break;
                    case 0xA7:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 178);
                        }
                        break;
                    case 0xA8:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 172);
                        }
                        break;
                    case 0xA9:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 176);
                        }
                        break;
                    case 0xAD:
                        if (source != nullptr)
                            goto LABEL_159;
                        break;
                    case 0xAF:
                        if (source != nullptr)
                        {
                            v20 = LaunchOn((mode.m_world != nullptr ? mode.m_world->m_player : nullptr), 185);
                            if ( v20 )
                                if (v20 != nullptr && source != nullptr) {
                                    v20->SendMsg(v20, 14, reinterpret_cast<msgparam_t>(&source->m_pos), 0, 0);
                                }
                        }
                        break;
                    case 0xC4:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 215);
                        }
                        break;
                    case 0xC9:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 214);
                        }
                        break;
                    case 0xCD:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 213);
                        }
                        break;
                    case 0xD3:
                        if (target != nullptr)
                        {
                            v23 = LaunchOn(target, 274);
                            if ( v23 )
                            {
                                if (v23 != nullptr && target != nullptr) {
                                    v23->SendMsg(v23, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
                                }
                            }
                            LaunchOn(target, 268);
                        }
                        break;
                    case 0xD6:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 276);
                        }
                        break;
                    case 0xD7:
                        goto L_159137_0;
                    case 0xD8:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 270);
                        }
                        break;
                    case 0xD9:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 271);
                        }
                        break;
                    case 0xDA:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 272);
                        }
                        break;
                    case 0xE7:
                        if (source != nullptr)
                        {
                            if (target != nullptr)
                            {
                                v25 = LaunchOn(source, 299);
                                v26 = v25;
                                if ( v25 )
                                {
                                    if (v26 != nullptr && target != nullptr) {
                                        v26->SendMsg(v26, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
                                    }
                                    if (v26 != nullptr) {
                                        v26->SendMsg(v26, 44, clampedLevel, 0, 0);
                                    }
                                }
                            }
                        }
                        break;
                    case 0xEA:
                    case 0xEB:
                    case 0xEC:
                    case 0xED:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 300);
                        }
                        break;
                    case 0xF9:
                        goto L_159049;
                    case 0xFC:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 252);
                        }
                        break;
                    case 0xFF:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 251);
                        }
                        break;
                    case 0x100:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 248);
                        }
                        break;
                    case 0x101:
                        goto L_159068;
                    case 0x102:
                        if (source != nullptr)
                        {
                            if (source != nullptr) {
                                source->SendMsg(source, 109, 250, 0, 0);
                            }
                            LaunchOn(source, 250);
                        }
                        break;
                    case 0x106:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 253);
                        }
                        break;
                    case 0x10C:
                        if (source != nullptr)
                        {
                            PlayUseSkillWave("effect\\T_\xC4\xA1\xC0\xD7.wav", 0.0, 0.0, 0.0);
                            mode.SendMsg(135, 0, 0, 0);
                            if (source != nullptr) {
                                source->SendMsg(source, 109, 254, 0, 0);
                            }
                            LaunchOn(source, 254);
                            LaunchOn(source, 261);
                            source->SetForceAnimation(96, 0, 0, 96, 0, 1, 0x14, 0);
                        }
                        break;
                    case 0x10D:
                        if (source != nullptr)
                            source->SetForceAnimation(96, 3, 3, 96, 3, 1, 0xBB8, 0);
                        break;
                    case 0x10E:
                        if (source != nullptr)
                        {
                            mode.SendMsg(135, 0, 0, 0);
                            PlayUseSkillWave("effect\\T_\xB8\xB6\xB9\xFD\xB9\xDD\xBB\xE7.wav", 0.0, 0.0, 0.0);
                            if (source != nullptr) {
                                source->SendMsg(source, 109, 203, 0, 0);
                            }
                            LaunchOn(source, 203);
                            LaunchOn(source, 261);
                            source->m_forceAnimSpeed = static_cast<u8>(0x1Eu);
                            source->SetForceAnimation(96, 1, 2, 96, 2, 1, 0x3E8, 0);
                        }
                        break;
                    case 0x114:
                        if (source != nullptr)
                        {
                            LaunchOn(source, 244);
                        }
                        break;
                    case 0x115:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 234);
                        }
                        break;
                    case 0x118:
                        goto L_159037;
                    case 0x119:
                        goto L_159040;
                    case 0x11A:
                        goto L_159043;
                    case 0x11B:
                        goto L_159046_0;
                    case 0x121:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 235);
                        }
                        break;
                    case 0x132:
                        PlayUseSkillWave("effect\\\xC7\xEC\xB8\xA3\xB8\xF0\xB5\xE5\xC0\xC7 \xC1\xF6\xC6\xCE\xC0\xCC.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x133:
                        PlayUseSkillWave("effect\\priest_slowpoison.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x134:
                        PlayUseSkillWave("effect\\ac_concentration.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x135:
                        PlayUseSkillWave("effect\\T_\xC7\xC7\xB8\xB5.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x136:
                        PlayUseSkillWave("effect\\\xB4\xD9\xB4\xD9\xB9\xCC\xB5\xDA\xC1\xFD\xB1\xE2.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x137:
                        PlayUseSkillWave("effect\\\xC5\xA9\xB7\xA1\xC4\xBF.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x138:
                        PlayUseSkillWave("effect\\\xC7\xC3\xB8\xB3.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x139:
                        PlayUseSkillWave("effect\\2008cat.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x13E:
                        if (source != nullptr)
                        {
                            source->SetForceAnimation(96, 4, 5, 96, 4, 1, 0x708, 1);
                            if ( source == (mode.m_world != nullptr ? mode.m_world->m_player : nullptr) )
                            {
                                LaunchOn(source, 295);
                            }
                        }
                        break;
                    case 0x140:
                        PlayUseSkillWave("dream_device_edq.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x141:
                        PlayUseSkillWave("effect\\vallentine.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x142:
                        PlayUseSkillWave("amon_ra_die01.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x146:
                        if (source != nullptr)
                        {
                            source->SetForceAnimation(96, 0, 0, 96, 0, 1, 0x4B0, 0);
                            if ( source == (mode.m_world != nullptr ? mode.m_world->m_player : nullptr) )
                            {
                                LaunchOn(source, 296);
                            }
                        }
                        break;
                    case 0x147:
                        PlayUseSkillWave("effect\\ef_entry.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x148:
                        PlayUseSkillWave("effect\\h_defence.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x149:
                        PlayUseSkillWave("effect\\h_castling.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x14A:
                        PlayUseSkillWave("effect\\\xB9\xF6\xBC\xAD\xC5\xA9.wav", 0.0, 0.0, 0.0);
                        break;
                    case 0x14F:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 253);
                            if (target != nullptr) {
                                target->SendMsg(target, 88, rawLevel, -16776961, 114);
                            }
                        }
                        break;
                    case 0x156:
                        if (target != nullptr)
                        {
                            LaunchOn(target, 453);
                        }
                        break;
                        return;
                }
            }
        }
}

