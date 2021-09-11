# mp4player  
RV1109平台上实现一个简单的 mp4 播放器。  
base目录包含一些基础框架实现，包含信号，线程，时间等  
media目录包含mp4播放的实现  
ui目录，简单的QT UI，为了显示视频。  

# 依赖
rkmedia:用来播放声音，以及rga的一些东西。  
mpp：用来解码 h264/h265  
ffmpeg：用来解码音频  
libevent：实现一个简单的消息泵  
QT：用来显示视频  
以上库均已包含在rv1109固件中  
