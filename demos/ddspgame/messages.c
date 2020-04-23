/*---------------------------------------------------------------------*
        Copyright (C) 1998 Nintendo.
        
        $RCSfile: messages.c,v $
        $Revision: 1.8 $
        $Date: 1998/11/13 12:23:49 $
 *---------------------------------------------------------------------*/
#include <ultra64.h>

#include "main.h"
#include "font.h"

u8	*message[ERROR_MES_SIZE] = {
#ifdef _ASCII_
  "Normal termination",
  "Abnormal termination",
  "Forced termination",
  "Error number",
  "Please refer to the User's Guide.",
  "[Caution] Please do not remove the disk", 
  "while the access lamp is blinking.",
  "Disk is not inserted.",
  "Please insert the disk again.",
  "Is the disk inserted?",
  "Please insert the disk.",
  "Wrong disk may have been inserted.",
  "Please confirm and",
  "set the time.",
  "Refer to the User's Guide",
  "for details",
  "Is memory expansion pak",
  "inserted correctly?",
  "Currently creating.",
  "Fail to save the previous data",
  "perfectly.",
  "Data will be deleted.",
  "Please push A button.",
  "Please remove the disk.",
  "Change the disk and",
  "insert the right disk.",
#else
"����I��", 						
"�ُ�I��",
"�����I��",
"�G���[�ԍ�",
"�戵�����������ǂ݂��������B",
"�y���Ӂz�A�N�Z�X�����v�_�Œ���",
"�f�B�X�N�𔲂��Ȃ��ł��������B",
"�f�B�X�N���������܂�Ă��܂���",
"���������B",
"�f�B�X�N���������܂�Ă��܂����B",
 "�f�B�X�N����������ł��������B", 
 "�Ԉ�����f�B�X�N���������܂�Ă���",
 "�\��������܂��B",
 "������ݒ肵�Ă��������B",
 "�ڂ����́A�戵��������",
 "���ǂ݂��������B",               
 "�������[�g���p�b�N��������",
 "��������ł���܂����H",
 "���ݍ쐬���ł��B",
 "�O��f�[�^���Ō�܂ł������",
 "�Z�[�u�ł��܂���ł����B",
 "�f�[�^�������܂��B",
 "�`�{�^���������Ă��������B",
 "�f�B�X�N�����o���Ă��������B",
 "�������f�B�X�N�Ɍ�������",
 "�f�B�X�N���������݂Ȃ����Ă��������B",
#endif
};

s32	kaddr[ERROR_MES_SIZE];

