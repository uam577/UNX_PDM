#!/usr/bin/ksh
##########################################################################
#
#  NOMBRE DEL SCRIPT: bloquea_usu.sh
#  DESARROLLADO POR: uam577
#  FECHA: 25/10/2017
#
#  MODIFICADO POR:
#  FECHA:
#
#  DESCRIPCION: Script para bloquear usuarios en caso de emergencia.
#
#  PARAMETROS:
#    $1 -  Ambito en el que se van a bloquear los usuarios, puede ser:
#              Una maquina: [<Servidor>]
#              Un fichero conteniendo un grupo de maquinas [ -s <Fichero_Servidores> ]
#              Una especificacisn de SO: [ "HPUX", "SOLARIS" o "RHEL" ]
#              Toda la plataforma: [ "ALL" ]
#    $2 -  Usuario o usuarios a bloquear, puede ser:
#              Un usuario: [ <Usuario> ]
#              Un fichero conteniendo un grupo de usuarios: [ -u <Fichero_Usuarios> ]
#
#    Tanto <Fichero_Servidores> como <Fichero_Usuarios> contendran unicamente un servidor o usuario por linea.
#
#  FICHEROS UTILIZADOS
#
#  EJEMPLO DE USO:
#    USAGE: bloquea_usu.sh [ -s <Fichero_Servidores> ][ <Servidor> | "HPUX" | "SOLARIS" | "RHEL" | "ALL" ]  [ -u <Fichero_Usuarios> ] [ <Usuario> ]
#
#  DESCRIPCION DE LOS CODIGOS DE RETORNO UTILIZADOS POR EL SCRIPT:
#
#    0 - Finalizacion corecta
#    1 - El numero de parametros no es valido.
#    2 -
#
##########################################################################
#
# JOB CONTROL-M:
#
##########################################################################
 
# ESTABLECEMOS UN ENTORNO ADECUADO PARA EL SCRIPT
##########################################################################
 
export PATH=$PATH:/usr/bin:/usr/sbin
export TERM=vt100
 
# DEFINIMOS COMO VARIABLES CON PATH COMPLETO LOS COMANDOS QUE VAMOS A USAR
##########################################################################
 
export ECHO=/usr/bin/echo
export DATE=/usr/bin/date
export GREP=/usr/bin/grep
export AWK=/usr/bin/awk
export CAT=/usr/bin/cat
export TEE="/usr/bin/tee -a"
export SSH=/usr/bin/ssh
 
# DEFINICION DE LAS VARIABLES UTILIZADAS POR EL SCRIPT
##########################################################################
 
# export VAR1=valor1  # Definicion de la variable
export FICH_MAQ="/SD/scrges/admin/config/servidores/" # Fichero por defecto del que se van a tomar las maquinas.
export MAQUINAS=""           # Maquinas sobre las que se actuara
export USUARIOS=""               # Usuarios sobre los que se actuara
 
# DEFINICION DE FUNCIONES UTILIZADAS POR EL SCRIPT
##########################################################################
 
# Repetir el bloque para cada funcion definida
 
#=========================================================================
#
#  NOMBRE DE LA FUNCION: usage()
#  DESARROLLADO POR: uam577
#  FECHA: 21/09/2017
#
#  DESCRIPCION:
#  Saca por pantalla el usage del script ante un error en la invocacion.
#
#  PARAMETROS:
#  No tiene.
#
#  EJEMPLO DE USO:
#
#  usage
#
#=========================================================================
 
function usage {
 
        $ECHO "USAGE: $0 <Servidor>|<Fichero_Servidores>|"HPUX"|"SOLARIS"|"RHEL"|"ALL"   <Usuario>|<Fichero_Usuarios> "
    $ECHO ""
        $ECHO "  Ambito en el que se van a bloquear los usuarios, puede ser:"
    $ECHO "     Una maquina: <Servidor>"
    $ECHO "     Un fichero conteniendo un grupo de maquinas <Fichero_Servidores>"
        $ECHO "       En este caso el fichero contendra unicamente el nombre o IP de un servidor por lmnea."
    $ECHO '     Una especificacisn de SO: "HPUX", "SOLARIS" o "RHEL" '
    $ECHO '     Toda la plataforma: "ALL" '
        $ECHO "  Usuario o usuarios a bloquear, puede ser:"
    $ECHO "     Un usuario: <Usuario>"
    $ECHO "     Un fichero conteniendo un grupo de usuarios: <Fichero_Usuarios>"
        $ECHO "       En este caso el fichero contendra unicamente el nombre o ID de un usuario por lmnea."
        exit 1
 
} #function usage ()
 
#=========================================================================
#
#  NOMBRE DE LA FUNCION: respuesta()
#  DESARROLLADO POR: uam577
#  FECHA: 2/07/2013
#
#  DESCRIPCION:
#  Recibe por pantalla la respuesta del usuario. Si no es s, si, sI, S, Si
#  o SI sale con un exit 3
#
#  PARAMETROS:
#  No tiene.
#
#  EJEMPLO DE USO:
#
#  respuesta
#
#=========================================================================
 
function respuesta {
    RESPUESTA=no
    read RESPUESTA
    case $RESPUESTA in
        s)
        ;;
        S)
        ;;
        si)
        ;;
        SI)
        ;;
        Si)
        ;;
        sI)
        ;;
        *)
        exit 3
        ;;
    esac
} # function respuesta
 
#=========================================================================
 
# COMPROBACION DE QUE RECIBE EL NUMERO ADECUADO DE PARAMETROS
##########################################################################
 
# En el caso de que un determinado numero no sea valido se debera borrar
# la linea correspondiente del case
 
case $# in
  2) ;;
  *) $ECHO "ERROR: El numero de parametros no es adecuado"
     $ECHO "USAGE: $0  <Servidor>|<Fichero_Servidores>|"HPUX"|"SOLARIS"|"RHEL"|"ALL"    <Fichero_Usuarios>|<Usuario> "
     exit 1
     ;;
esac
 
# INICIO DEL BLOQUE PRINCIPAL DEL SCRIPT
##########################################################################
 
AMBITO=$1
USER=$2
 
if [ -f $USER ]
then
    USUARIOS=`$CAT $USER |$GREP -v ^# | $AWK -F: '{print$1}'`
else
    USUARIOS=$USER
fi
 
case "$AMBITO" in
    HPUX)
             MAQUINAS=`$CAT $FICH_MAQ | $GREP 'HPUX' | $GREP -v ^# | $AWK -F: '{print$1}'`
        ;;
        SOLARIS)
             MAQUINAS=`$CAT $FICH_MAQ | $GREP 'ORACLE' | $GREP -v ^# | $AWK -F: '{print$1}'`
        ;;
        RHEL)
                 MAQUINAS=`$CAT $FICH_MAQ | $GREP 'LINUX RHEL' | $GREP -v ^# | $AWK -F: '{print$1}'`
        ;;
        ALL)
                 MAQUINAS=`$CAT $FICH_MAQ |$GREP -v ^# | $AWK -F: '{print$1}'`
        ;;
        *)
                 if [ -f $AMBITO ]
                 then
                   MAQUINAS=`$CAT $AMBITO`
                 else
                   MAQUINAS=$1
                 fi
        ;;
esac
 
for MAQ in $MAQUINAS
do
     for USU in $USUARIOS
     do
         #$ECHO "$SSH $MAQ passwd -l $USU"
         #$SSH $MAQ "passwd -l $USU" &
     done
done
 
# En caso de que no hayamos salido antes por una condicion de error acabamos aqui
 
exit 0
