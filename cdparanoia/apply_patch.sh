#!/bin/sh

if [ -f "cdparanoia-III-10.2.src.tgz" ]; then
   echo "File cdparanoia-III-10.2.src.tgz already exists."
else
   wget http://downloads.xiph.org/releases/cdparanoia/cdparanoia-III-10.2.src.tgz
fi

if [ -d "cdparanoia-III-10.2" ]; then
  echo "Directory cdparanoia-III-10.2 already exists"
else
  tar zxvf cdparanoia-III-10.2.src.tgz
  cp cdparanoia-III-10.2/interface/cdda_interface.h cdparanoia-III-10.2/interface/cdda_interface.h.org
  cp cdparanoia-III-10.2/interface/cooked_interface.c cdparanoia-III-10.2/interface/cooked_interface.c.org
  cp cdparanoia-III-10.2/interface/interface.c cdparanoia-III-10.2/interface/interface.c.org
  cp cdparanoia-III-10.2/interface/scan_devices.c cdparanoia-III-10.2/interface/scan_devices.c.org
  cp cdparanoia-III-10.2/interface/scsi_interface.c cdparanoia-III-10.2/interface/scsi_interface.c.org
  patch -p0 < cdda_interface.h.patch
  patch -p0 < cooked_interface.c.patch
  patch -p0 < interface.c.patch
  patch -p0 < scan_devices.c.patch
  patch -p0 < scsi_interface.c.patch
fi


