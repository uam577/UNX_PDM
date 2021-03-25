#!/usr/bin/ksh
################################################################
#                                                              #
# Script para obtener datos de usuarios locales                #
#							                                                       #
#  DESARROLLADO POR: uam577                                    #
#  FECHA: 2021                                                 #           
#                                                              #
################################################################
#
# Resto del programa
 
UID=$1
if [ "x$UID" = "x" ] ; then
 
        echo
        echo "ERROR: necesario un parametro, el login o uid a buscar. Por ejemplo: "
        echo
        echo "      $0 alex"
        echo "      $0 655321"
        echo
        echo "El ultimo uid asignado es $ultimo"
        echo
        exit 1
 
fi # if UID=""
echo
echo
echo "Buscando usuarios locales con UID \"$UID\" en maquinas...\c"
for i in `/usr/bin/cat /path/admin/config/servidores/ | egrep -v ^ | awk -F: '{print$1}'`; do
        echo ".\c"
        RLINEA=`ssh $i -n "/bin/cat /etc/passwd | /bin/awk -F: '{print\\$1,\\$3,\\$4}' | /bin/egrep '"^$UID "|" $UID$"'" 2>/dev/null`
 
        # Opciones: maquina no valida (no existe/no se llega), no hallado o hallado.
        if echo "$RLINEA" | egrep -i "permission denied|unknown host|timed out">/dev/null ; then
                echo " $i no valido.\c"
        elif [ "$RLINEA" = "" ] ; then
                echo ".\c"
        else
                echo
                echo "Encontrado en $i: "
                echo "$RLINEA" | sed 's/^/      /g'
                echo "Siguiendo la busqueda...\c"
        fi
done
echo
echo "***************************************************"
echo
echo "Busqueda finalizada."
echo
echo "***************************************************"
echo
