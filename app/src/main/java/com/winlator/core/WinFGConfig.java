package com.winlator.core;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * WinFGConfig — win-fg 帧生成引擎配置管理工具
 *
 * Why:  win-fg 通过环境变量控制行为，分散在容器的 envVars 字符串中。
 *       本类集中管理 WIN_FG_* 变量的读取/写入/移除，避免 UI 层直接拼字符串。
 * What: 仅操作 envVars 字符串，不触碰任何渲染/共存逻辑。
 * How:  调用 apply(envVars) 返回修改后的字符串；调用 reset(envVars) 移除所有 WIN_FG_* 变量。
 */
public class WinFGConfig {

    // win-fg 环境变量名（与上游 The412Banner/win-fg config.hpp 一致）
    public static final String VAR_ENABLE      = "WIN_FG_ENABLE";
    public static final String VAR_PERF_PRESET = "WIN_FG_PERF_PRESET";
    public static final String VAR_MULTIPLIER  = "WIN_FG_MULTIPLIER";
    public static final String VAR_DEBUG       = "WIN_FG_DEBUG";

    // 性能档位（对应 win-fg flowFinestForPreset: 0→1, 1→2, 2→3）
    public static final int PRESET_QUALITY   = 0; // 质量优先（光流最精细）
    public static final int PRESET_BALANCED  = 1; // 平衡（默认）
    public static final int PRESET_PERFORMANCE = 2; // 性能优先（光流最粗，延迟最低）

    public static final String[] PRESET_LABELS = {"质量", "平衡", "性能"};

    // 倍率（win-fg 支持 2x/3x/4x）
    public static final int[] MULTIPLIER_VALUES = {2, 3, 4};
    public static final String[] MULTIPLIER_LABELS = {"2x", "3x", "4x"};

    private boolean enabled = true;
    private int perfPreset = PRESET_BALANCED;
    private int multiplier = 2;
    private boolean debug = false;

    public WinFGConfig() {}

    /**
     * 从 envVars 字符串解析 win-fg 配置
     */
    public static WinFGConfig fromEnvVars(String envVars) {
        WinFGConfig cfg = new WinFGConfig();
        if (envVars == null || envVars.isEmpty()) return cfg;

        Map<String, String> vars = parseEnvVars(envVars);
        cfg.enabled = "1".equals(vars.get(VAR_ENABLE));
        if (vars.containsKey(VAR_PERF_PRESET)) {
            try { cfg.perfPreset = Integer.parseInt(vars.get(VAR_PERF_PRESET)); }
            catch (NumberFormatException ignored) {}
        }
        if (vars.containsKey(VAR_MULTIPLIER)) {
            try { cfg.multiplier = Integer.parseInt(vars.get(VAR_MULTIPLIER)); }
            catch (NumberFormatException ignored) {}
        }
        cfg.debug = "1".equals(vars.get(VAR_DEBUG));
        return cfg;
    }

    /**
     * 将当前配置应用到 envVars 字符串，返回新字符串
     */
    public String apply(String envVars) {
        Map<String, String> vars = parseEnvVars(envVars != null ? envVars : "");

        // 先移除所有 WIN_FG_* 变量
        vars.keySet().removeIf(k -> k.startsWith("WIN_FG_"));

        if (enabled) {
            vars.put(VAR_ENABLE, "1");
            vars.put(VAR_PERF_PRESET, String.valueOf(perfPreset));
            vars.put(VAR_MULTIPLIER, String.valueOf(multiplier));
            if (debug) vars.put(VAR_DEBUG, "1");
        }

        return formatEnvVars(vars);
    }

    /**
     * 从 envVars 中移除所有 WIN_FG_* 变量（回滚用）
     */
    public static String reset(String envVars) {
        if (envVars == null || envVars.isEmpty()) return envVars;
        Map<String, String> vars = parseEnvVars(envVars);
        vars.keySet().removeIf(k -> k.startsWith("WIN_FG_"));
        return formatEnvVars(vars);
    }

    // ---- Getters / Setters ----
    public boolean isEnabled() { return enabled; }
    public void setEnabled(boolean enabled) { this.enabled = enabled; }

    public int getPerfPreset() { return perfPreset; }
    public void setPerfPreset(int preset) { this.perfPreset = preset; }

    public int getMultiplier() { return multiplier; }
    public void setMultiplier(int multiplier) { this.multiplier = multiplier; }

    public boolean isDebug() { return debug; }
    public void setDebug(boolean debug) { this.debug = debug; }

    /** 获取倍率在 MULTIPLIER_VALUES 数组中的索引 */
    public int getMultiplierIndex() {
        for (int i = 0; i < MULTIPLIER_VALUES.length; i++)
            if (MULTIPLIER_VALUES[i] == multiplier) return i;
        return 0;
    }

    // ---- 内部工具 ----

    /** 解析 "A=1 B=2 C=3" 格式的 envVars 字符串为有序 Map */
    private static Map<String, String> parseEnvVars(String envVars) {
        Map<String, String> result = new LinkedHashMap<>();
        if (envVars == null || envVars.isEmpty()) return result;
        // 按空格分割，但值中可能包含路径（无空格），简单分割即可
        for (String token : envVars.split("\\s+")) {
            if (token.isEmpty()) continue;
            int eq = token.indexOf('=');
            if (eq > 0) {
                result.put(token.substring(0, eq), token.substring(eq + 1));
            }
        }
        return result;
    }

    /** 将 Map 格式化为 "A=1 B=2 C=3" 字符串 */
    private static String formatEnvVars(Map<String, String> vars) {
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> e : vars.entrySet()) {
            if (sb.length() > 0) sb.append(' ');
            sb.append(e.getKey()).append('=').append(e.getValue());
        }
        return sb.toString();
    }
}
