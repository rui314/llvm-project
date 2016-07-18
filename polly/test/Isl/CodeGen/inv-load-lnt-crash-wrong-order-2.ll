; RUN: opt %loadPolly -polly-codegen -S < %s | FileCheck %s
;
; This crashed our codegen at some point, verify it runs through
;
; CHECK: polly.start
;
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"

%struct.EqState.41.74.107.272.602.635.701.734.767.899.998.1229.2449.2482.2647.2680.2779.2911.3010.3036 = type { %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035*, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018*, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035* }
%struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018 = type { i32, %struct.Production.29.62.95.260.590.623.689.722.755.887.986.1217.2437.2470.2635.2668.2767.2899.2998.3014*, i32, i32, i32, i32, %struct.anon.0.30.63.96.261.591.624.690.723.756.888.987.1218.2438.2471.2636.2669.2768.2900.2999.3015, %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021*, %struct.Code.31.64.97.262.592.625.691.724.757.889.988.1219.2439.2472.2637.2670.2769.2901.3000.3016, %struct.Code.31.64.97.262.592.625.691.724.757.889.988.1219.2439.2472.2637.2670.2769.2901.3000.3016, %struct.anon.1.32.65.98.263.593.626.692.725.758.890.989.1220.2440.2473.2638.2671.2770.2902.3001.3017, i32, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018* }
%struct.Production.29.62.95.260.590.623.689.722.755.887.986.1217.2437.2470.2635.2668.2767.2899.2998.3014 = type { i8*, i32, %struct.anon.9.42.75.240.570.603.669.702.735.867.966.1197.2417.2450.2615.2648.2747.2879.2978.3011, i32, i8, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018*, [8 x %struct.Production.29.62.95.260.590.623.689.722.755.887.986.1217.2437.2470.2635.2668.2767.2899.2998.3014*], [8 x %struct.Declaration.13.46.79.244.574.607.673.706.739.871.970.1201.2421.2454.2619.2652.2751.2883.2982.3012*], %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035*, %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021*, %struct.Term.18.51.84.249.579.612.678.711.744.876.975.1206.2426.2459.2624.2657.2756.2888.2987.3013*, %struct.Production.29.62.95.260.590.623.689.722.755.887.986.1217.2437.2470.2635.2668.2767.2899.2998.3014* }
%struct.anon.9.42.75.240.570.603.669.702.735.867.966.1197.2417.2450.2615.2648.2747.2879.2978.3011 = type { i32, i32, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018**, [3 x %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018*] }
%struct.Declaration.13.46.79.244.574.607.673.706.739.871.970.1201.2421.2454.2619.2652.2751.2883.2982.3012 = type { %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021*, i32, i32 }
%struct.Term.18.51.84.249.579.612.678.711.744.876.975.1206.2426.2459.2624.2657.2756.2888.2987.3013 = type { i32, i32, i32, i32, i32, i8*, i32, i8, %struct.Production.29.62.95.260.590.623.689.722.755.887.986.1217.2437.2470.2635.2668.2767.2899.2998.3014* }
%struct.anon.0.30.63.96.261.591.624.690.723.756.888.987.1218.2438.2471.2636.2669.2768.2900.2999.3015 = type { i32, i32, %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021**, [3 x %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021*] }
%struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021 = type { i32, i32, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018*, %union.anon.11.44.77.242.572.605.671.704.737.869.968.1199.2419.2452.2617.2650.2749.2881.2980.3020 }
%union.anon.11.44.77.242.572.605.671.704.737.869.968.1199.2419.2452.2617.2650.2749.2881.2980.3020 = type { %struct.Unresolved.10.43.76.241.571.604.670.703.736.868.967.1198.2418.2451.2616.2649.2748.2880.2979.3019 }
%struct.Unresolved.10.43.76.241.571.604.670.703.736.868.967.1198.2418.2451.2616.2649.2748.2880.2979.3019 = type { i8*, i32 }
%struct.Code.31.64.97.262.592.625.691.724.757.889.988.1219.2439.2472.2637.2670.2769.2901.3000.3016 = type { i8*, i32 }
%struct.anon.1.32.65.98.263.593.626.692.725.758.890.989.1220.2440.2473.2638.2671.2770.2902.3001.3017 = type { i32, i32, %struct.Code.31.64.97.262.592.625.691.724.757.889.988.1219.2439.2472.2637.2670.2769.2901.3000.3016**, [3 x %struct.Code.31.64.97.262.592.625.691.724.757.889.988.1219.2439.2472.2637.2670.2769.2901.3000.3016*] }
%struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035 = type { i32, i64, %struct.anon.2.14.47.80.245.575.608.674.707.740.872.971.1202.2422.2455.2620.2653.2752.2884.2983.3022, %struct.anon.3.15.48.81.246.576.609.675.708.741.873.972.1203.2423.2456.2621.2654.2753.2885.2984.3023, %struct.VecGoto.17.50.83.248.578.611.677.710.743.875.974.1205.2425.2458.2623.2656.2755.2887.2986.3025, %struct.VecAction.20.53.86.251.581.614.680.713.746.878.977.1208.2428.2461.2626.2659.2758.2890.2989.3027, %struct.VecAction.20.53.86.251.581.614.680.713.746.878.977.1208.2428.2461.2626.2659.2758.2890.2989.3027, %struct.VecHint.22.55.88.253.583.616.682.715.748.880.979.1210.2430.2463.2628.2661.2760.2892.2991.3029, %struct.VecHint.22.55.88.253.583.616.682.715.748.880.979.1210.2430.2463.2628.2661.2760.2892.2991.3029, %struct.Scanner.27.60.93.258.588.621.687.720.753.885.984.1215.2435.2468.2633.2666.2765.2897.2996.3034, i8, i8*, i32, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035*, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035*, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018*, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018* }
%struct.anon.2.14.47.80.245.575.608.674.707.740.872.971.1202.2422.2455.2620.2653.2752.2884.2983.3022 = type { i32, i32, %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021**, [3 x %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021*] }
%struct.anon.3.15.48.81.246.576.609.675.708.741.873.972.1203.2423.2456.2621.2654.2753.2885.2984.3023 = type { i32, i32, %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021**, [3 x %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021*] }
%struct.VecGoto.17.50.83.248.578.611.677.710.743.875.974.1205.2425.2458.2623.2656.2755.2887.2986.3025 = type { i32, i32, %struct.Goto.16.49.82.247.577.610.676.709.742.874.973.1204.2424.2457.2622.2655.2754.2886.2985.3024**, [3 x %struct.Goto.16.49.82.247.577.610.676.709.742.874.973.1204.2424.2457.2622.2655.2754.2886.2985.3024*] }
%struct.Goto.16.49.82.247.577.610.676.709.742.874.973.1204.2424.2457.2622.2655.2754.2886.2985.3024 = type { %struct.Elem.12.45.78.243.573.606.672.705.738.870.969.1200.2420.2453.2618.2651.2750.2882.2981.3021*, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035* }
%struct.VecAction.20.53.86.251.581.614.680.713.746.878.977.1208.2428.2461.2626.2659.2758.2890.2989.3027 = type { i32, i32, %struct.Action.19.52.85.250.580.613.679.712.745.877.976.1207.2427.2460.2625.2658.2757.2889.2988.3026**, [3 x %struct.Action.19.52.85.250.580.613.679.712.745.877.976.1207.2427.2460.2625.2658.2757.2889.2988.3026*] }
%struct.Action.19.52.85.250.580.613.679.712.745.877.976.1207.2427.2460.2625.2658.2757.2889.2988.3026 = type { i32, %struct.Term.18.51.84.249.579.612.678.711.744.876.975.1206.2426.2459.2624.2657.2756.2888.2987.3013*, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018*, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035*, i32, i8* }
%struct.VecHint.22.55.88.253.583.616.682.715.748.880.979.1210.2430.2463.2628.2661.2760.2892.2991.3029 = type { i32, i32, %struct.Hint.21.54.87.252.582.615.681.714.747.879.978.1209.2429.2462.2627.2660.2759.2891.2990.3028**, [3 x %struct.Hint.21.54.87.252.582.615.681.714.747.879.978.1209.2429.2462.2627.2660.2759.2891.2990.3028*] }
%struct.Hint.21.54.87.252.582.615.681.714.747.879.978.1209.2429.2462.2627.2660.2759.2891.2990.3028 = type { i32, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035*, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018* }
%struct.Scanner.27.60.93.258.588.621.687.720.753.885.984.1215.2435.2468.2633.2666.2765.2897.2996.3034 = type { %struct.VecScanState.25.58.91.256.586.619.685.718.751.883.982.1213.2433.2466.2631.2664.2763.2895.2994.3032, %struct.VecScanStateTransition.26.59.92.257.587.620.686.719.752.884.983.1214.2434.2467.2632.2665.2764.2896.2995.3033 }
%struct.VecScanState.25.58.91.256.586.619.685.718.751.883.982.1213.2433.2466.2631.2664.2763.2895.2994.3032 = type { i32, i32, %struct.ScanState.24.57.90.255.585.618.684.717.750.882.981.1212.2432.2465.2630.2663.2762.2894.2993.3031**, [3 x %struct.ScanState.24.57.90.255.585.618.684.717.750.882.981.1212.2432.2465.2630.2663.2762.2894.2993.3031*] }
%struct.ScanState.24.57.90.255.585.618.684.717.750.882.981.1212.2432.2465.2630.2663.2762.2894.2993.3031 = type { i32, [256 x %struct.ScanState.24.57.90.255.585.618.684.717.750.882.981.1212.2432.2465.2630.2663.2762.2894.2993.3031*], %struct.VecAction.20.53.86.251.581.614.680.713.746.878.977.1208.2428.2461.2626.2659.2758.2890.2989.3027, %struct.VecAction.20.53.86.251.581.614.680.713.746.878.977.1208.2428.2461.2626.2659.2758.2890.2989.3027, [256 x %struct.ScanStateTransition.23.56.89.254.584.617.683.716.749.881.980.1211.2431.2464.2629.2662.2761.2893.2992.3030*] }
%struct.ScanStateTransition.23.56.89.254.584.617.683.716.749.881.980.1211.2431.2464.2629.2662.2761.2893.2992.3030 = type { i32, %struct.VecAction.20.53.86.251.581.614.680.713.746.878.977.1208.2428.2461.2626.2659.2758.2890.2989.3027, %struct.VecAction.20.53.86.251.581.614.680.713.746.878.977.1208.2428.2461.2626.2659.2758.2890.2989.3027 }
%struct.VecScanStateTransition.26.59.92.257.587.620.686.719.752.884.983.1214.2434.2467.2632.2665.2764.2896.2995.3033 = type { i32, i32, %struct.ScanStateTransition.23.56.89.254.584.617.683.716.749.881.980.1211.2431.2464.2629.2662.2761.2893.2992.3030**, [3 x %struct.ScanStateTransition.23.56.89.254.584.617.683.716.749.881.980.1211.2431.2464.2629.2662.2761.2893.2992.3030*] }

; Function Attrs: nounwind
declare noalias i8* @malloc() #0

; Function Attrs: nounwind uwtable
define void @build_eq() #1 {
entry:
  %call = tail call noalias i8* @malloc() #2
  %0 = bitcast i8* %call to %struct.EqState.41.74.107.272.602.635.701.734.767.899.998.1229.2449.2482.2647.2680.2779.2911.3010.3036*
  br label %for.cond.preheader

for.cond.preheader:                               ; preds = %for.cond.preheader, %entry
  br i1 undef, label %for.cond.260.preheader, label %for.cond.preheader

for.cond.260.preheader:                           ; preds = %for.cond.preheader
  br i1 undef, label %for.cond.316.preheader, label %for.body.265

for.cond.316.preheader:                           ; preds = %for.cond.260.preheader
  br i1 undef, label %for.cond.400.preheader, label %for.body.321

for.body.265:                                     ; preds = %for.cond.260.preheader
  unreachable

for.cond.400.preheader:                           ; preds = %for.inc.397, %for.cond.316.preheader
  ret void

for.body.321:                                     ; preds = %for.inc.397, %for.cond.316.preheader
  %1 = load %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035*, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035** undef, align 8
  %eq329 = getelementptr inbounds %struct.EqState.41.74.107.272.602.635.701.734.767.899.998.1229.2449.2482.2647.2680.2779.2911.3010.3036, %struct.EqState.41.74.107.272.602.635.701.734.767.899.998.1229.2449.2482.2647.2680.2779.2911.3010.3036* %0, i64 0, i32 0
  br i1 undef, label %for.inc.397, label %land.lhs.true.331

land.lhs.true.331:                                ; preds = %for.body.321
  br i1 undef, label %for.inc.397, label %if.then.334

if.then.334:                                      ; preds = %land.lhs.true.331
  %2 = load %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018*, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018** undef, align 8
  br i1 undef, label %for.inc.397, label %land.lhs.true.369

land.lhs.true.369:                                ; preds = %if.then.334
  %n380 = getelementptr inbounds %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018, %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018* %2, i64 0, i32 6, i32 0
  %3 = load i32, i32* %n380, align 8
  br i1 true, label %if.then.383, label %for.inc.397

if.then.383:                                      ; preds = %land.lhs.true.369
  %reduces_with387 = getelementptr inbounds %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035* %1, i64 0, i32 15
  %4 = bitcast %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018** %reduces_with387 to i64*
  %5 = load %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035*, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035** %eq329, align 8
  %index389 = getelementptr inbounds %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035, %struct.State.28.61.94.259.589.622.688.721.754.886.985.1216.2436.2469.2634.2667.2766.2898.2997.3035* %5, i64 0, i32 0
  %6 = load i32, i32* %index389, align 8
  store i32 0, i32* %index389, align 8
  %diff_rule392 = getelementptr inbounds %struct.EqState.41.74.107.272.602.635.701.734.767.899.998.1229.2449.2482.2647.2680.2779.2911.3010.3036, %struct.EqState.41.74.107.272.602.635.701.734.767.899.998.1229.2449.2482.2647.2680.2779.2911.3010.3036* %0, i64 0, i32 1
  %7 = bitcast %struct.Rule.33.66.99.264.594.627.693.726.759.891.990.1221.2441.2474.2639.2672.2771.2903.3002.3018** %diff_rule392 to i64*
  br label %for.inc.397

for.inc.397:                                      ; preds = %if.then.383, %land.lhs.true.369, %if.then.334, %land.lhs.true.331, %for.body.321
  br i1 undef, label %for.body.321, label %for.cond.400.preheader
}
