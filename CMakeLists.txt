
set(edge264_src
    edge264.c
    edge264_deblock.c
    edge264.h
    edge264_inter.c
    edge264_internal.h
    edge264_intra.c
    edge264_mvpred.c
    edge264_residual.c
    edge264_slice.c
#    edge264_test.c
#    edge264_unit_tests.c
  )  
add_library(edge264 ${edge264_src})
target_compile_options(edge264 PRIVATE "-std=gnu99;-mssse3;-flax-vector-conversions")
target_include_directories(edge264 PUBLIC .)
add_library(edge264::edge264 ALIAS edge264)


# I suppose opengl needed only for play function.
#GLFW3 = `pkg-config --cflags --static --libs glfw3`
# 


