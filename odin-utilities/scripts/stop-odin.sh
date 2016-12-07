PATH_LOGS="/home/fran/bisdn/spring-odin-patch/scripts"

docker stop $(docker ps -a -q)
docker rm $(docker ps -a -q)
iw dev mon0 del
rm $PATH_LOGS/odin.log
#rm $PATH_LOGS/hostapd.log
rm $PATH_LOGS/xdpd_output.log
