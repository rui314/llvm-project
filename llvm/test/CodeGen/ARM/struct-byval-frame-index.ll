; RUN: llc < %s -mcpu=cortex-a15 -verify-machineinstrs | FileCheck %s

; Check a spill right after a function call with large struct byval is correctly
; generated.
; PR16393

; CHECK: set_stored_macroblock_parameters
; CHECK: str r{{.*}}, [sp, [[SLOT:#[0-9]+]]] @ 4-byte Spill
; CHECK: bl RestoreMVBlock8x8
; CHECK: bl RestoreMVBlock8x8
; CHECK: bl RestoreMVBlock8x8
; CHECK: ldr r{{.*}}, [sp, [[SLOT]]] @ 4-byte Reload

target triple = "armv7l-unknown-linux-gnueabihf"

%struct.RD_DATA.1.145.289.433.545.689.881.897.929.977.1025.1057.1089.1169.1281.2673.2689.2705.2721.2737.2753.2769.2785.2801.2849.2865.2881.2897.2913.2929.2945.2961.2977.2993.3009.3025.3041.3057.3073.3089.3169.3249.3265.3393.3441 = type { double, [16 x [16 x i16]], [16 x [16 x i16]], [16 x [16 x i16]], i32****, i32***, i32, i16, [4 x i32], [4 x i32], i8**, [16 x i8], [16 x i8], i32, i64, i32, i16******, i16******, [2 x [4 x [4 x i8]]], i32, i32, i32, i32, i32, i32, i32, i32, i32 }
%struct.ImageParameters.12.156.300.444.556.700.892.908.940.988.1036.1068.1100.1180.1292.2684.2700.2716.2732.2748.2764.2780.2796.2812.2860.2876.2892.2908.2924.2940.2956.2972.2988.3004.3020.3036.3052.3068.3084.3100.3180.3260.3276.3404.3452 = type { i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, float, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i8**, i8**, i32, i32***, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, [9 x [16 x [16 x i16]]], [5 x [16 x [16 x i16]]], [9 x [8 x [8 x i16]]], [2 x [4 x [16 x [16 x i16]]]], [16 x [16 x i16]], [16 x [16 x i32]], i32****, i32***, i32***, i32***, i32****, i32****, %struct.Picture.9.153.297.441.553.697.889.905.937.985.1033.1065.1097.1177.1289.2681.2697.2713.2729.2745.2761.2777.2793.2809.2857.2873.2889.2905.2921.2937.2953.2969.2985.3001.3017.3033.3049.3065.3081.3097.3177.3257.3273.3401.3449*, %struct.Slice.8.152.296.440.552.696.888.904.936.984.1032.1064.1096.1176.1288.2680.2696.2712.2728.2744.2760.2776.2792.2808.2856.2872.2888.2904.2920.2936.2952.2968.2984.3000.3016.3032.3048.3064.3080.3096.3176.3256.3272.3400.3448*, %struct.macroblock.10.154.298.442.554.698.890.906.938.986.1034.1066.1098.1178.1290.2682.2698.2714.2730.2746.2762.2778.2794.2810.2858.2874.2890.2906.2922.2938.2954.2970.2986.3002.3018.3034.3050.3066.3082.3098.3178.3258.3274.3402.3450*, i32*, i32*, i32, i32, i32, i32, [4 x [4 x i32]], i32, i32, i32, i32, i32, double, i32, i32, i32, i32, i16******, i16******, i16******, i16******, [15 x i16], i32, i32, i32, i32, i32, i32, i32, i32, [6 x [32 x i32]], i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, [1 x i32], i32, i32, [2 x i32], i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, %struct.DecRefPicMarking_s.11.155.299.443.555.699.891.907.939.987.1035.1067.1099.1179.1291.2683.2699.2715.2731.2747.2763.2779.2795.2811.2859.2875.2891.2907.2923.2939.2955.2971.2987.3003.3019.3035.3051.3067.3083.3099.3179.3259.3275.3403.3451*, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, double**, double***, i32***, double**, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, [3 x [2 x i32]], [2 x i32], i32, i32, i16, i32, i32, i32, i32, i32 }
%struct.Picture.9.153.297.441.553.697.889.905.937.985.1033.1065.1097.1177.1289.2681.2697.2713.2729.2745.2761.2777.2793.2809.2857.2873.2889.2905.2921.2937.2953.2969.2985.3001.3017.3033.3049.3065.3081.3097.3177.3257.3273.3401.3449 = type { i32, i32, [100 x %struct.Slice.8.152.296.440.552.696.888.904.936.984.1032.1064.1096.1176.1288.2680.2696.2712.2728.2744.2760.2776.2792.2808.2856.2872.2888.2904.2920.2936.2952.2968.2984.3000.3016.3032.3048.3064.3080.3096.3176.3256.3272.3400.3448*], i32, float, float, float }
%struct.Slice.8.152.296.440.552.696.888.904.936.984.1032.1064.1096.1176.1288.2680.2696.2712.2728.2744.2760.2776.2792.2808.2856.2872.2888.2904.2920.2936.2952.2968.2984.3000.3016.3032.3048.3064.3080.3096.3176.3256.3272.3400.3448 = type { i32, i32, i32, i32, i32, i32, %struct.datapartition.4.148.292.436.548.692.884.900.932.980.1028.1060.1092.1172.1284.2676.2692.2708.2724.2740.2756.2772.2788.2804.2852.2868.2884.2900.2916.2932.2948.2964.2980.2996.3012.3028.3044.3060.3076.3092.3172.3252.3268.3396.3444*, %struct.MotionInfoContexts.6.150.294.438.550.694.886.902.934.982.1030.1062.1094.1174.1286.2678.2694.2710.2726.2742.2758.2774.2790.2806.2854.2870.2886.2902.2918.2934.2950.2966.2982.2998.3014.3030.3046.3062.3078.3094.3174.3254.3270.3398.3446*, %struct.TextureInfoContexts.7.151.295.439.551.695.887.903.935.983.1031.1063.1095.1175.1287.2679.2695.2711.2727.2743.2759.2775.2791.2807.2855.2871.2887.2903.2919.2935.2951.2967.2983.2999.3015.3031.3047.3063.3079.3095.3175.3255.3271.3399.3447*, i32, i32*, i32*, i32*, i32, i32*, i32*, i32*, i32 (i32)*, [3 x [2 x i32]] }
%struct.datapartition.4.148.292.436.548.692.884.900.932.980.1028.1060.1092.1172.1284.2676.2692.2708.2724.2740.2756.2772.2788.2804.2852.2868.2884.2900.2916.2932.2948.2964.2980.2996.3012.3028.3044.3060.3076.3092.3172.3252.3268.3396.3444 = type { %struct.Bitstream.2.146.290.434.546.690.882.898.930.978.1026.1058.1090.1170.1282.2674.2690.2706.2722.2738.2754.2770.2786.2802.2850.2866.2882.2898.2914.2930.2946.2962.2978.2994.3010.3026.3042.3058.3074.3090.3170.3250.3266.3394.3442*, %struct.EncodingEnvironment.3.147.291.435.547.691.883.899.931.979.1027.1059.1091.1171.1283.2675.2691.2707.2723.2739.2755.2771.2787.2803.2851.2867.2883.2899.2915.2931.2947.2963.2979.2995.3011.3027.3043.3059.3075.3091.3171.3251.3267.3395.3443, %struct.EncodingEnvironment.3.147.291.435.547.691.883.899.931.979.1027.1059.1091.1171.1283.2675.2691.2707.2723.2739.2755.2771.2787.2803.2851.2867.2883.2899.2915.2931.2947.2963.2979.2995.3011.3027.3043.3059.3075.3091.3171.3251.3267.3395.3443 }
%struct.Bitstream.2.146.290.434.546.690.882.898.930.978.1026.1058.1090.1170.1282.2674.2690.2706.2722.2738.2754.2770.2786.2802.2850.2866.2882.2898.2914.2930.2946.2962.2978.2994.3010.3026.3042.3058.3074.3090.3170.3250.3266.3394.3442 = type { i32, i32, i8, i32, i32, i8, i8, i32, i32, i8*, i32 }
%struct.EncodingEnvironment.3.147.291.435.547.691.883.899.931.979.1027.1059.1091.1171.1283.2675.2691.2707.2723.2739.2755.2771.2787.2803.2851.2867.2883.2899.2915.2931.2947.2963.2979.2995.3011.3027.3043.3059.3075.3091.3171.3251.3267.3395.3443 = type { i32, i32, i32, i32, i32, i8*, i32*, i32, i32 }
%struct.MotionInfoContexts.6.150.294.438.550.694.886.902.934.982.1030.1062.1094.1174.1286.2678.2694.2710.2726.2742.2758.2774.2790.2806.2854.2870.2886.2902.2918.2934.2950.2966.2982.2998.3014.3030.3046.3062.3078.3094.3174.3254.3270.3398.3446 = type { [3 x [11 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [2 x [9 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [2 x [10 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [2 x [6 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [4 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445], [4 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445], [3 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445] }
%struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445 = type { i16, i8, i32 }
%struct.TextureInfoContexts.7.151.295.439.551.695.887.903.935.983.1031.1063.1095.1175.1287.2679.2695.2711.2727.2743.2759.2775.2791.2807.2855.2871.2887.2903.2919.2935.2951.2967.2983.2999.3015.3031.3047.3063.3079.3095.3175.3255.3271.3399.3447 = type { [2 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445], [4 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445], [3 x [4 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [10 x [4 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [10 x [15 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [10 x [15 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [10 x [5 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [10 x [5 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [10 x [15 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]], [10 x [15 x %struct.BiContextType.5.149.293.437.549.693.885.901.933.981.1029.1061.1093.1173.1285.2677.2693.2709.2725.2741.2757.2773.2789.2805.2853.2869.2885.2901.2917.2933.2949.2965.2981.2997.3013.3029.3045.3061.3077.3093.3173.3253.3269.3397.3445]] }
%struct.macroblock.10.154.298.442.554.698.890.906.938.986.1034.1066.1098.1178.1290.2682.2698.2714.2730.2746.2762.2778.2794.2810.2858.2874.2890.2906.2922.2938.2954.2970.2986.3002.3018.3034.3050.3066.3082.3098.3178.3258.3274.3402.3450 = type { i32, i32, i32, [2 x i32], i32, [8 x i32], %struct.macroblock.10.154.298.442.554.698.890.906.938.986.1034.1066.1098.1178.1290.2682.2698.2714.2730.2746.2762.2778.2794.2810.2858.2874.2890.2906.2922.2938.2954.2970.2986.3002.3018.3034.3050.3066.3082.3098.3178.3258.3274.3402.3450*, %struct.macroblock.10.154.298.442.554.698.890.906.938.986.1034.1066.1098.1178.1290.2682.2698.2714.2730.2746.2762.2778.2794.2810.2858.2874.2890.2906.2922.2938.2954.2970.2986.3002.3018.3034.3050.3066.3082.3098.3178.3258.3274.3402.3450*, i32, [2 x [4 x [4 x [2 x i32]]]], [16 x i8], [16 x i8], i32, i64, [4 x i32], [4 x i32], i64, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i16, double, i32, i32, i32, i32, i32, i32, i32, i32, i32 }
%struct.DecRefPicMarking_s.11.155.299.443.555.699.891.907.939.987.1035.1067.1099.1179.1291.2683.2699.2715.2731.2747.2763.2779.2795.2811.2859.2875.2891.2907.2923.2939.2955.2971.2987.3003.3019.3035.3051.3067.3083.3099.3179.3259.3275.3403.3451 = type { i32, i32, i32, i32, i32, %struct.DecRefPicMarking_s.11.155.299.443.555.699.891.907.939.987.1035.1067.1099.1179.1291.2683.2699.2715.2731.2747.2763.2779.2795.2811.2859.2875.2891.2907.2923.2939.2955.2971.2987.3003.3019.3035.3051.3067.3083.3099.3179.3259.3275.3403.3451* }
%struct.storable_picture.13.157.301.445.557.701.893.909.941.989.1037.1069.1101.1181.1293.2685.2701.2717.2733.2749.2765.2781.2797.2813.2861.2877.2893.2909.2925.2941.2957.2973.2989.3005.3021.3037.3053.3069.3085.3101.3181.3261.3277.3405.3453 = type { i32, i32, i32, i32, i32, i32, [6 x [33 x i64]], [6 x [33 x i64]], [6 x [33 x i64]], [6 x [33 x i64]], i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i16**, i16****, i16****, i16*****, i16***, i8*, i8***, i64***, i64***, i16****, i8**, i8**, %struct.storable_picture.13.157.301.445.557.701.893.909.941.989.1037.1069.1101.1181.1293.2685.2701.2717.2733.2749.2765.2781.2797.2813.2861.2877.2893.2909.2925.2941.2957.2973.2989.3005.3021.3037.3053.3069.3085.3101.3181.3261.3277.3405.3453*, %struct.storable_picture.13.157.301.445.557.701.893.909.941.989.1037.1069.1101.1181.1293.2685.2701.2717.2733.2749.2765.2781.2797.2813.2861.2877.2893.2909.2925.2941.2957.2973.2989.3005.3021.3037.3053.3069.3085.3101.3181.3261.3277.3405.3453*, %struct.storable_picture.13.157.301.445.557.701.893.909.941.989.1037.1069.1101.1181.1293.2685.2701.2717.2733.2749.2765.2781.2797.2813.2861.2877.2893.2909.2925.2941.2957.2973.2989.3005.3021.3037.3053.3069.3085.3101.3181.3261.3277.3405.3453*, i32, i32, i32, i32, i32, i32, i32 }
%struct.RD_8x8DATA.15.159.303.447.559.703.895.911.943.991.1039.1071.1103.1183.1295.2687.2703.2719.2735.2751.2767.2783.2799.2815.2863.2879.2895.2911.2927.2943.2959.2975.2991.3007.3023.3039.3055.3071.3087.3103.3183.3263.3279.3407.3455 = type { i32, [16 x [16 x i32]], [16 x [16 x i32]], [16 x [16 x i32]], [3 x [16 x [16 x i32]]], [4 x i16], [4 x i8], [4 x i8], [4 x i8], [16 x [16 x i16]], [16 x [16 x i16]], [16 x [16 x i32]] }

@cofAC = external global i32****, align 4
@cofDC = external global i32***, align 4
@rdopt = external global %struct.RD_DATA.1.145.289.433.545.689.881.897.929.977.1025.1057.1089.1169.1281.2673.2689.2705.2721.2737.2753.2769.2785.2801.2849.2865.2881.2897.2913.2929.2945.2961.2977.2993.3009.3025.3041.3057.3073.3089.3169.3249.3265.3393.3441*, align 4
@img = external global %struct.ImageParameters.12.156.300.444.556.700.892.908.940.988.1036.1068.1100.1180.1292.2684.2700.2716.2732.2748.2764.2780.2796.2812.2860.2876.2892.2908.2924.2940.2956.2972.2988.3004.3020.3036.3052.3068.3084.3100.3180.3260.3276.3404.3452*
@enc_picture = external global %struct.storable_picture.13.157.301.445.557.701.893.909.941.989.1037.1069.1101.1181.1293.2685.2701.2717.2733.2749.2765.2781.2797.2813.2861.2877.2893.2909.2925.2941.2957.2973.2989.3005.3021.3037.3053.3069.3085.3101.3181.3261.3277.3405.3453*
@si_frame_indicator = external global i32, align 4
@sp2_frame_indicator = external global i32, align 4
@lrec = external global i32**, align 4
@tr8x8 = external global %struct.RD_8x8DATA.15.159.303.447.559.703.895.911.943.991.1039.1071.1103.1183.1295.2687.2703.2719.2735.2751.2767.2783.2799.2815.2863.2879.2895.2911.2927.2943.2959.2975.2991.3007.3023.3039.3055.3071.3087.3103.3183.3263.3279.3407.3455, align 4
@best_mode = external global i16, align 2
@best_c_imode = external global i32, align 4
@best_i16offset = external global i32, align 4
@bi_pred_me = external global i16, align 2
@b8mode = external global [4 x i32], align 4
@b8pdir = external global [4 x i32], align 4
@b4_intra_pred_modes = external global [16 x i8], align 1
@b8_intra_pred_modes8x8 = external global [16 x i8], align 1
@b4_ipredmode = external global [16 x i8], align 1
@b8_ipredmode8x8 = external global [4 x [4 x i8]], align 1
@rec_mbY = external global [16 x [16 x i16]], align 2
@lrec_rec = external global [16 x [16 x i32]], align 4
@rec_mbU = external global [16 x [16 x i16]], align 2
@rec_mbV = external global [16 x [16 x i16]], align 2
@lrec_rec_U = external global [16 x [16 x i32]], align 4
@lrec_uv = external global i32***, align 4
@lrec_rec_V = external global [16 x [16 x i32]], align 4
@cbp = external global i32, align 4
@cbp_blk = external global i64, align 8
@luma_transform_size_8x8_flag = external global i32, align 4
@frefframe = external global [4 x [4 x i8]], align 1
@brefframe = external global [4 x [4 x i8]], align 1

; Function Attrs: nounwind
declare void @llvm.memcpy.p0i8.p0i8.i32(i8* nocapture, i8* nocapture, i32, i32, i1) #0

; Function Attrs: nounwind
declare void @llvm.memset.p0i8.i32(i8* nocapture, i8, i32, i32, i1) #0

; Function Attrs: nounwind
declare void @SetMotionVectorsMB(%struct.macroblock.10.154.298.442.554.698.890.906.938.986.1034.1066.1098.1178.1290.2682.2698.2714.2730.2746.2762.2778.2794.2810.2858.2874.2890.2906.2922.2938.2954.2970.2986.3002.3018.3034.3050.3066.3082.3098.3178.3258.3274.3402.3450* nocapture, i32) #1

; Function Attrs: nounwind
define void @set_stored_macroblock_parameters() #1 {
entry:
  %0 = load %struct.ImageParameters.12.156.300.444.556.700.892.908.940.988.1036.1068.1100.1180.1292.2684.2700.2716.2732.2748.2764.2780.2796.2812.2860.2876.2892.2908.2924.2940.2956.2972.2988.3004.3020.3036.3052.3068.3084.3100.3180.3260.3276.3404.3452** @img, align 4, !tbaa !0
  %1 = load i32* undef, align 4, !tbaa !3
  %mb_data = getelementptr inbounds %struct.ImageParameters.12.156.300.444.556.700.892.908.940.988.1036.1068.1100.1180.1292.2684.2700.2716.2732.2748.2764.2780.2796.2812.2860.2876.2892.2908.2924.2940.2956.2972.2988.3004.3020.3036.3052.3068.3084.3100.3180.3260.3276.3404.3452* %0, i32 0, i32 61
  %2 = load %struct.macroblock.10.154.298.442.554.698.890.906.938.986.1034.1066.1098.1178.1290.2682.2698.2714.2730.2746.2762.2778.2794.2810.2858.2874.2890.2906.2922.2938.2954.2970.2986.3002.3018.3034.3050.3066.3082.3098.3178.3258.3274.3402.3450** %mb_data, align 4, !tbaa !0
  br label %for.body

for.body:                                         ; preds = %for.body, %entry
  br i1 undef, label %for.end, label %for.body

for.end:                                          ; preds = %for.body
  br i1 undef, label %for.body20, label %if.end

for.body20:                                       ; preds = %for.end
  unreachable

if.end:                                           ; preds = %for.end
  br i1 undef, label %if.end40, label %for.cond31.preheader

for.cond31.preheader:                             ; preds = %if.end
  unreachable

if.end40:                                         ; preds = %if.end
  br i1 undef, label %if.end43, label %if.then42

if.then42:                                        ; preds = %if.end40
  br label %if.end43

if.end43:                                         ; preds = %if.then42, %if.end40
  br i1 undef, label %if.end164, label %for.cond47.preheader

for.cond47.preheader:                             ; preds = %if.end43
  br i1 undef, label %for.body119, label %if.end164

for.body119:                                      ; preds = %for.body119, %for.cond47.preheader
  br i1 undef, label %for.body119, label %if.end164

if.end164:                                        ; preds = %for.body119, %for.cond47.preheader, %if.end43
  store i32*** null, i32**** @cofDC, align 4, !tbaa !0
  %mb_type = getelementptr inbounds %struct.macroblock.10.154.298.442.554.698.890.906.938.986.1034.1066.1098.1178.1290.2682.2698.2714.2730.2746.2762.2778.2794.2810.2858.2874.2890.2906.2922.2938.2954.2970.2986.3002.3018.3034.3050.3066.3082.3098.3178.3258.3274.3402.3450* %2, i32 %1, i32 8
  br i1 undef, label %if.end230, label %if.then169

if.then169:                                       ; preds = %if.end164
  br i1 undef, label %for.cond185.preheader, label %for.cond210.preheader

for.cond185.preheader:                            ; preds = %if.then169
  unreachable

for.cond210.preheader:                            ; preds = %if.then169
  unreachable

if.end230:                                        ; preds = %if.end164
  tail call void @llvm.memcpy.p0i8.p0i8.i32(i8* undef, i8* bitcast ([4 x i32]* @b8mode to i8*), i32 16, i32 4, i1 false)
  %b8pdir = getelementptr inbounds %struct.macroblock.10.154.298.442.554.698.890.906.938.986.1034.1066.1098.1178.1290.2682.2698.2714.2730.2746.2762.2778.2794.2810.2858.2874.2890.2906.2922.2938.2954.2970.2986.3002.3018.3034.3050.3066.3082.3098.3178.3258.3274.3402.3450* %2, i32 %1, i32 15
  %3 = bitcast [4 x i32]* %b8pdir to i8*
  tail call void @llvm.memcpy.p0i8.p0i8.i32(i8* %3, i8* bitcast ([4 x i32]* @b8pdir to i8*), i32 16, i32 4, i1 false)
  br i1 undef, label %if.end236, label %if.then233

if.then233:                                       ; preds = %if.end230
  unreachable

if.end236:                                        ; preds = %if.end230
  %cmp242 = icmp ne i16 undef, 8
  %4 = load i32* @luma_transform_size_8x8_flag, align 4, !tbaa !3
  %tobool245 = icmp ne i32 %4, 0
  %or.cond812 = or i1 %cmp242, %tobool245
  br i1 %or.cond812, label %if.end249, label %land.lhs.true246

land.lhs.true246:                                 ; preds = %if.end236
  br i1 undef, label %if.end249, label %if.then248

if.then248:                                       ; preds = %land.lhs.true246
  tail call void @RestoreMVBlock8x8(i32 1, i32 0, %struct.RD_8x8DATA.15.159.303.447.559.703.895.911.943.991.1039.1071.1103.1183.1295.2687.2703.2719.2735.2751.2767.2783.2799.2815.2863.2879.2895.2911.2927.2943.2959.2975.2991.3007.3023.3039.3055.3071.3087.3103.3183.3263.3279.3407.3455* byval @tr8x8, i32 0) #0
  tail call void @RestoreMVBlock8x8(i32 1, i32 2, %struct.RD_8x8DATA.15.159.303.447.559.703.895.911.943.991.1039.1071.1103.1183.1295.2687.2703.2719.2735.2751.2767.2783.2799.2815.2863.2879.2895.2911.2927.2943.2959.2975.2991.3007.3023.3039.3055.3071.3087.3103.3183.3263.3279.3407.3455* byval @tr8x8, i32 0) #0
  tail call void @RestoreMVBlock8x8(i32 1, i32 3, %struct.RD_8x8DATA.15.159.303.447.559.703.895.911.943.991.1039.1071.1103.1183.1295.2687.2703.2719.2735.2751.2767.2783.2799.2815.2863.2879.2895.2911.2927.2943.2959.2975.2991.3007.3023.3039.3055.3071.3087.3103.3183.3263.3279.3407.3455* byval @tr8x8, i32 0) #0
  br label %if.end249

if.end249:                                        ; preds = %if.then248, %land.lhs.true246, %if.end236
  %5 = load i32* @luma_transform_size_8x8_flag, align 4, !tbaa !3
  %6 = load %struct.RD_DATA.1.145.289.433.545.689.881.897.929.977.1025.1057.1089.1169.1281.2673.2689.2705.2721.2737.2753.2769.2785.2801.2849.2865.2881.2897.2913.2929.2945.2961.2977.2993.3009.3025.3041.3057.3073.3089.3169.3249.3265.3393.3441** @rdopt, align 4, !tbaa !0
  %luma_transform_size_8x8_flag264 = getelementptr inbounds %struct.RD_DATA.1.145.289.433.545.689.881.897.929.977.1025.1057.1089.1169.1281.2673.2689.2705.2721.2737.2753.2769.2785.2801.2849.2865.2881.2897.2913.2929.2945.2961.2977.2993.3009.3025.3041.3057.3073.3089.3169.3249.3265.3393.3441* %6, i32 0, i32 21
  store i32 %5, i32* %luma_transform_size_8x8_flag264, align 4, !tbaa !3
  %7 = load i32* undef, align 4, !tbaa !3
  %add281 = add nsw i32 %7, 0
  br label %for.body285

for.body285:                                      ; preds = %for.inc503, %if.end249
  %8 = phi %struct.ImageParameters.12.156.300.444.556.700.892.908.940.988.1036.1068.1100.1180.1292.2684.2700.2716.2732.2748.2764.2780.2796.2812.2860.2876.2892.2908.2924.2940.2956.2972.2988.3004.3020.3036.3052.3068.3084.3100.3180.3260.3276.3404.3452* [ undef, %if.end249 ], [ %.pre1155, %for.inc503 ]
  %i.21103 = phi i32 [ 0, %if.end249 ], [ %inc504, %for.inc503 ]
  %block_x286 = getelementptr inbounds %struct.ImageParameters.12.156.300.444.556.700.892.908.940.988.1036.1068.1100.1180.1292.2684.2700.2716.2732.2748.2764.2780.2796.2812.2860.2876.2892.2908.2924.2940.2956.2972.2988.3004.3020.3036.3052.3068.3084.3100.3180.3260.3276.3404.3452* %8, i32 0, i32 37
  %9 = load i32* %block_x286, align 4, !tbaa !3
  %add287 = add nsw i32 %9, %i.21103
  %shr289 = ashr i32 %i.21103, 1
  %add290 = add nsw i32 %shr289, 0
  %arrayidx292 = getelementptr inbounds %struct.macroblock.10.154.298.442.554.698.890.906.938.986.1034.1066.1098.1178.1290.2682.2698.2714.2730.2746.2762.2778.2794.2810.2858.2874.2890.2906.2922.2938.2954.2970.2986.3002.3018.3034.3050.3066.3082.3098.3178.3258.3274.3402.3450* %2, i32 %1, i32 15, i32 %add290
  %10 = load %struct.storable_picture.13.157.301.445.557.701.893.909.941.989.1037.1069.1101.1181.1293.2685.2701.2717.2733.2749.2765.2781.2797.2813.2861.2877.2893.2909.2925.2941.2957.2973.2989.3005.3021.3037.3053.3069.3085.3101.3181.3261.3277.3405.3453** @enc_picture, align 4, !tbaa !0
  %ref_idx = getelementptr inbounds %struct.storable_picture.13.157.301.445.557.701.893.909.941.989.1037.1069.1101.1181.1293.2685.2701.2717.2733.2749.2765.2781.2797.2813.2861.2877.2893.2909.2925.2941.2957.2973.2989.3005.3021.3037.3053.3069.3085.3101.3181.3261.3277.3405.3453* %10, i32 0, i32 35
  %11 = load i8**** %ref_idx, align 4, !tbaa !0
  %12 = load i8*** %11, align 4, !tbaa !0
  %arrayidx313 = getelementptr inbounds i8** %12, i32 %add281
  %13 = load i8** %arrayidx313, align 4, !tbaa !0
  %arrayidx314 = getelementptr inbounds i8* %13, i32 %add287
  store i8 -1, i8* %arrayidx314, align 1, !tbaa !1
  %14 = load %struct.ImageParameters.12.156.300.444.556.700.892.908.940.988.1036.1068.1100.1180.1292.2684.2700.2716.2732.2748.2764.2780.2796.2812.2860.2876.2892.2908.2924.2940.2956.2972.2988.3004.3020.3036.3052.3068.3084.3100.3180.3260.3276.3404.3452** @img, align 4, !tbaa !0
  %MbaffFrameFlag327 = getelementptr inbounds %struct.ImageParameters.12.156.300.444.556.700.892.908.940.988.1036.1068.1100.1180.1292.2684.2700.2716.2732.2748.2764.2780.2796.2812.2860.2876.2892.2908.2924.2940.2956.2972.2988.3004.3020.3036.3052.3068.3084.3100.3180.3260.3276.3404.3452* %14, i32 0, i32 100
  %15 = load i32* %MbaffFrameFlag327, align 4, !tbaa !3
  %tobool328 = icmp eq i32 %15, 0
  br i1 %tobool328, label %if.end454, label %if.then329

if.then329:                                       ; preds = %for.body285
  %16 = load %struct.RD_DATA.1.145.289.433.545.689.881.897.929.977.1025.1057.1089.1169.1281.2673.2689.2705.2721.2737.2753.2769.2785.2801.2849.2865.2881.2897.2913.2929.2945.2961.2977.2993.3009.3025.3041.3057.3073.3089.3169.3249.3265.3393.3441** @rdopt, align 4, !tbaa !0
  br label %if.end454

if.end454:                                        ; preds = %if.then329, %for.body285
  %17 = load i32* %arrayidx292, align 4, !tbaa !3
  %cmp457 = icmp eq i32 %17, 0
  br i1 %cmp457, label %if.then475, label %lor.lhs.false459

lor.lhs.false459:                                 ; preds = %if.end454
  %18 = load i32* %mb_type, align 4, !tbaa !3
  switch i32 %18, label %for.inc503 [
    i32 9, label %if.then475
    i32 10, label %if.then475
    i32 13, label %if.then475
    i32 14, label %if.then475
  ]

if.then475:                                       ; preds = %lor.lhs.false459, %lor.lhs.false459, %lor.lhs.false459, %lor.lhs.false459, %if.end454
  store i16 0, i16* undef, align 2, !tbaa !4
  br label %for.inc503

for.inc503:                                       ; preds = %if.then475, %lor.lhs.false459
  %inc504 = add nsw i32 %i.21103, 1
  %.pre1155 = load %struct.ImageParameters.12.156.300.444.556.700.892.908.940.988.1036.1068.1100.1180.1292.2684.2700.2716.2732.2748.2764.2780.2796.2812.2860.2876.2892.2908.2924.2940.2956.2972.2988.3004.3020.3036.3052.3068.3084.3100.3180.3260.3276.3404.3452** @img, align 4, !tbaa !0
  br label %for.body285
}

; Function Attrs: nounwind
declare void @update_offset_params(i32, i32) #1

; Function Attrs: nounwind
declare void @RestoreMVBlock8x8(i32, i32, %struct.RD_8x8DATA.15.159.303.447.559.703.895.911.943.991.1039.1071.1103.1183.1295.2687.2703.2719.2735.2751.2767.2783.2799.2815.2863.2879.2895.2911.2927.2943.2959.2975.2991.3007.3023.3039.3055.3071.3087.3103.3183.3263.3279.3407.3455* byval nocapture, i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-frame-pointer-elim-non-leaf"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "unsafe-fp-math"="false" "use-soft-float"="false" }

!0 = metadata !{metadata !"any pointer", metadata !1}
!1 = metadata !{metadata !"omnipotent char", metadata !2}
!2 = metadata !{metadata !"Simple C/C++ TBAA"}
!3 = metadata !{metadata !"int", metadata !1}
!4 = metadata !{metadata !"short", metadata !1}
