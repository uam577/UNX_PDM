################################################################################
#
# SCRIPT PARA CHEQUEO PREVIO Y POSTERIOR AL PARCHEADO DE MAQUINAS CON HP-UX 11i v3
#
# uam577 @ TSUNIX-PDM
#
################################################################################
#VARIABLES
#
HOST=`uname -n`
DATE=`date -u +%Y%m%d-%H%M%S`
machinfo | grep Virtual > /dev/null 2>&1
if [ $? -eq 0 ]; then MODEL=Virtual; else MODEL=Fisica; fi
if [ $MODEL != Virtual ]; then
DISK1=`vgdisplay -v vg00 | tail -17 | grep "PV Name" | head -1 | awk '{print$3}'` > /dev/null 2>&1
DISK2=`vgdisplay -v vg00 | tail -17 | grep "PV Name" | tail -1 | awk '{print$3}'` > /dev/null 2>&1
fi
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "COMPROBACION DE PROBLEMAS DE DISCO/LVOLES" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
ioscan -funNCdisk | grep NO > /dev/null 2>&1
if [ $? -eq 0 ]; then echo "Existen discos en NO_HW !" >> parches.$HOST-$DATE; else echo "Todos los discos en CLAIMED" >> parches.$HOST-$DATE; fi
vgdisplay -v vg00 |egrep 'stale|unavai' > /dev/null 2>&1
if [ $? -eq 0 ]; then echo "Hay problemas con algun disco/lvol del vg00 !" >> parches.$HOST-$DATE; else echo "Todos los discos/lvoles del vg00 available y syncd" >> parches.$HOST-$DATE; fi
echo "" >> parches.$HOST-$DATE
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "ORDEN DISCOS EN VG00" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
if [ $MODEL = Virtual ]; then echo "Maquina virtual con un solo disco en vg00" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
else
echo "" >> parches.$HOST-$DATE
vgdisplay -v vg00 | tail -17 >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
fi
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "SETBOOT" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
setboot >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "LVLNBOOT" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
lvlnboot -v 2> /dev/null >> parches.$HOST-$DATE
if [ $MODEL != Virtual ]; then
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "The physical volume key of a disk indicates its order in the volume group. The first physical volume has the key 0, the second has the key 1,
and so on. This need not be the order of appearance in /etc/lvmtab file although it is usually like that, at least when a volume group is initially created" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
vgcfgrestore -n vg00 -v -l >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
fi
if [ $MODEL != Virtual ]; then
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "ORDEN DE LVOLES EN LOS DISCOS" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo $DISK1>> parches.$HOST-$DATE
pvdisplay -v $DISK1 | grep 'current.*0000 $' >> parches.$HOST-$DATE
echo "---------------------" >> parches.$HOST-$DATE
echo $DISK2>> parches.$HOST-$DATE
pvdisplay -v $DISK2 | grep 'current.*0000 $' >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
fi
if [ $MODEL != Virtual ]; then
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "LVOLES CON MIRROR" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
for i in `vgdisplay -v vg00 | grep "LV Name" | awk '{print$3}'`
do
echo $i ; lvdisplay $i | grep -i mirror
done >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
fi
if [ $MODEL != Virtual ]; then
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "DISCO PRIMARIO Y ALTERNATIVO EN CADA LVOL" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
for i in `vgdisplay -v vg00 | grep "LV Name" | awk '{print$3}'`
do
echo $i; lvdisplay -v $i | grep 00000
done >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
fi
if [ $MODEL != Virtual ]; then
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "BOOTCONF" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
cat /stand/bootconf >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
fi
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "FILESET EN ESTADO DISTINTO DE "CONFIGURED"" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
/usr/sbin/swlist -l fileset -a state | /usr/bin/grep -v -e "#" -e "configured" >>parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "CHECK_PATCHES" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
/usr/contrib/bin/check_patches | grep RESULT>> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "##########################################################">> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "NIVEL DE PARCHES Y S.O." >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
/usr/sbin/swlist -l bundle| egrep 'OE|FEATU|HWE|QP|BUNDL'| sed 's/                              / /'>> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
echo "" >> parches.$HOST-$DATE
