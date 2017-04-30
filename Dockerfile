FROM ubuntu:14.04

MAINTAINER Ben Heasly <benjamin.heasly@gmail.com>

### headless X server dependencies
RUN apt-get update \
    && apt-get install -y \
    libx11-dev \
    libxxf86vm-dev \
    x11-xserver-utils \
    x11proto-xf86vidmode-dev \
    x11vnc \
    xpra \
    xserver-xorg-video-dummy \
    && apt-get clean \
    && apt-get autoclean \
    && apt-get autoremove

### mitsuba dependencies
RUN apt-get update \
    && apt-get install -y \
    build-essential \
    scons \
    mercurial \
    qt4-dev-tools \
    libpng12-dev \
    libjpeg-dev \
    libilmbase-dev \
    libxerces-c-dev \
    libboost-all-dev \
    libopenexr-dev \
    libglewmx-dev \
    libxxf86vm-dev \
    libpcrecpp0 \
    libeigen3-dev \
    libfftw3-dev \
    wget \
    && apt-get clean \
    && apt-get autoclean \
    && apt-get autoremove

### vnc x11
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
  git \
  libgl1-mesa-dri \
  menu \
  net-tools \
  openbox \
  python-pip \
  sudo \
  supervisor \
  tint2 \
  x11-xserver-utils \
  x11vnc \
  xinit \
  xserver-xorg-video-dummy \
  xserver-xorg-input-void \
  websockify && \
  rm -f /usr/share/applications/x11vnc.desktop && \
  pip install supervisor-stdout && \
  apt-get -y clean

### mitsuba git opengl dependencies
RUN apt-get update \
    && apt-get install -y \
    cmake \
    libgl1-mesa-dri \
    x11-xserver-utils \
    x11vnc \
    xinit \
    xserver-xorg-video-dummy \
    xserver-xorg-input-void \
    x11-apps mesa-utils

COPY etc/skel/.xinitrc /etc/skel/.xinitrc
RUN useradd -m -s /bin/bash user
USER user
RUN cp /etc/skel/.xinitrc /home/user/
USER root
RUN echo "user ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/user
RUN git clone https://github.com/kanaka/noVNC.git /opt/noVNC && \
cd /opt/noVNC && \
git checkout 6a90803feb124791960e3962e328aa3cfb729aeb && \
ln -s vnc_auto.html index.html
# noVNC (http server) is on 6080, and the VNC server is on 5900
EXPOSE 6080 5900
COPY etc /etc
COPY usr /usr
ENV DISPLAY :0

CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/supervisord.conf"]




WORKDIR /mitsuba
