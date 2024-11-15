# Copyright 2024 Nikos Leivadaris <nikosleiv@gmail.com>.
# SPDX-License-Identifier: MIT

KMOD=edu

SRCS=edu.c
SRCS+=edu_ctrl.c
SRCS+=device_if.h bus_if.h pci_if.h

DEBUG_FLAGS=-g

.include <bsd.kmod.mk>

.OBJDIR: ${.CURDIR}/../obj
