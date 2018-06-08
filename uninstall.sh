if [ -d _build ]; then
  cd _build
    sudo make uninstall
    make clean
    cd ..
    rm -rf _build
fi

