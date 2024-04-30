#!/bin/sh -x
#
#   ÉÜÅ[ÉUí«â¡  
#

# 
#  mkuser <user_name> <uid> <group_name>
# 
mkuser()
{
    USER=$1
    USER_ID=$2
    GROUP=$3

    mkdir /home/$USER
    useradd -u $USER_ID -g $GROUP -d /home/$USER -s /bin/tcsh $USER
    chown $USER:$GROUP /home/$USER

    # copy dot files
    echo "source ~oga/com_env" > /home/$USER/.cshrc
    cp /home/oga/.ueyamanrc /home/$USER
    cp /home/oga/.vimrc     /home/$USER
    chown $USER:$GROUP /home/$USER/.[cuv]*
    return
}

mkuser newuser 9001 users

