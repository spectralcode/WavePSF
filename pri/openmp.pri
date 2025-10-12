CONFIG += openmp
msvc {
	QMAKE_CFLAGS	+= /openmp
	QMAKE_CXXFLAGS	+= /openmp
} else {
	QMAKE_CFLAGS	+= -fopenmp
	QMAKE_CXXFLAGS	+= -fopenmp
	QMAKE_LFLAGS	+= -fopenmp
}