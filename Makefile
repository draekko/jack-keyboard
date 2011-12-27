VERSION?=2.7.1

help:
	@echo "Targets: configure all clean install package"

configure:

	rm -rf build

	mkdir build

	cd build ; cmake ..

all:
	make -C build all

install:
	make -C build install

clean:
	make -C build clean

package:
	make -C build clean || echo -n

	tar -cvf temp.tar --exclude="*~" --exclude="*#" \
		--exclude=".svn" --exclude="*.orig" --exclude="*.rej" \
		AUTHORS \
		CMakeLists.txt \
		COPYING \
		Makefile \
		NEWS \
		README \
		TODO \
		cmake \
		man \
		pixmaps \
		src

	rm -rf jack-keyboard-${VERSION}

	mkdir jack-keyboard-${VERSION}

	tar -xvf temp.tar -C jack-keyboard-${VERSION}

	rm -rf temp.tar

	tar -zcvf jack-keyboard-${VERSION}.tar.gz jack-keyboard-${VERSION}
