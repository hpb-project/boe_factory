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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "axu_connector.h"
#include "community.h"

#define fetch_axu_package_sequence() (g_sequence_id++)
int make_package(A_Package *ask, A_Package *res, u16 cmd)
{
	res->magic = ask->magic;
	res->cmd  = cmd;
	res->pid  = ask->pid;
	return 0;
}
