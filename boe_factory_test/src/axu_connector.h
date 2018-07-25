// Copyright 2018 The go-hpb Authors
// This file is part of the go-hpb.
//
// The go-hpb is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// The go-hpb is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with the go-hpb. If not, see <http://www.gnu.org/licenses/>.

#ifndef AXU_CONNECTOR_H
#define AXU_CONNECTOR_H
#include <xil_types.h>


#define PACKAGE_MAX_SIZE 	(256)

#define MAGIC (0xcc11)
#define REQ_SD_TEST      (0x1)
#define REQ_FLASH_TEST   (0x2)
#define REQ_DRAM_TEST    (0x3)
#define REQ_NET_TEST     (0x4)
#define REQ_CONNECT      (0x5)
#define REQ_RESULT       (0x6)
#define REQ_FINISH 		 (0x7)


#define RET_FAILED (0x1)
#define RET_SUCCES (0x2)

typedef struct A_PACKAGE{
    u16 magic;
    u16 cmd;
    u32 pid;
    u8  data[];
}A_Package;

int make_package(A_Package *ask, A_Package *res, u16 cmd);
#endif  /*AXU_CONNECTOR_H*/
