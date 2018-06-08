if [ -d _build ]; then
  rm -rf _build
fi

mkdir _build
cd _build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
sudo gtk-update-icon-cache /usr/share/icons/hicolor
