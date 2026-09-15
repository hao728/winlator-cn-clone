package com.winlator.core;

import android.content.Context;
import android.os.Environment;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * LogCaptureHelper — win-fg 帧生成日志捕获工具
 *
 * Why:  win-fg layer 运行在 guest Vulkan 进程中，日志通过 Android logcat 输出。
 *       用户需要一键导出 win-fg 相关日志用于调试，无需手动 adb logcat。
 * What: 执行 logcat 命令过滤 winfg/framegen/VK_LAYER 关键词，保存到外部存储。
 * How:  调用 capture(context) 返回保存的文件路径；日志自动脱敏（移除包名路径中的 UID）。
 *
 * 限制：普通应用只能捕获自身进程的 logcat（win-fg 在容器进程中，属于本应用），
 *       无法捕获其他应用日志。这是 Android 安全限制，无法绕过。
 */
public class LogCaptureHelper {

    // win-fg 日志关键词（与上游 layer.cpp 的 log tag 对齐）
    private static final String[] FILTER_KEYWORDS = {
        "winfg", "win_fg", "WIN_FG", "framegen", "frame.gen",
        "VK_LAYER_WIN", "optical.flow", "FSR3", "interpolat"
    };

    /**
     * 捕获 win-fg 相关日志并保存到外部存储
     * @return 保存的文件路径，失败返回 null
     */
    public static String capture(Context context) {
        try {
            // 1. 执行 logcat 抓取最近 5000 行
            Process process = Runtime.getRuntime().exec(
                new String[]{"logcat", "-d", "-t", "5000", "*:V"}
            );

            BufferedReader reader = new BufferedReader(
                new InputStreamReader(process.getInputStream())
            );

            StringBuilder filtered = new StringBuilder();
            String line;
            int matchCount = 0;

            // 2. 过滤关键词
            while ((line = reader.readLine()) != null) {
                String lower = line.toLowerCase();
                for (String kw : FILTER_KEYWORDS) {
                    if (lower.contains(kw.toLowerCase())) {
                        filtered.append(sanitize(line)).append('\n');
                        matchCount++;
                        break;
                    }
                }
            }
            reader.close();
            process.destroy();

            // 3. 写入文件
            String timestamp = new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(new Date());
            String fileName = "winfg_log_" + timestamp + ".txt";

            File outputFile;
            // 优先保存到 Download 目录（Android 10+ 用应用专属外部目录避免权限问题）
            File downloadDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
            if (downloadDir != null && downloadDir.canWrite()) {
                outputFile = new File(downloadDir, fileName);
            } else {
                // 回退到应用专属目录
                File appExternal = context.getExternalFilesDir(null);
                outputFile = new File(appExternal, fileName);
            }

            // 4. 添加文件头
            StringBuilder header = new StringBuilder();
            header.append("=== Winlator win-fg 帧生成调试日志 ===\n");
            header.append("导出时间: ").append(new Date()).append('\n');
            header.append("匹配行数: ").append(matchCount).append('\n');
            header.append("包名: ").append(context.getPackageName()).append('\n');
            header.append("说明: 本日志已自动脱敏（移除数字 UID）\n");
            header.append("====================================\n\n");

            try (FileOutputStream fos = new FileOutputStream(outputFile)) {
                fos.write(header.toString().getBytes());
                fos.write(filtered.toString().getBytes());
            }

            return outputFile.getAbsolutePath();

        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    /**
     * 日志脱敏：移除路径中的数字 UID（如 /data/user/0/ → /data/user/0/）
     * 实际上 Android 日志中包名路径不敏感，但移除纯数字 UID 更安全
     */
    private static String sanitize(String line) {
        // 移除 /data/user/0/ 后面的纯数字（UID），替换为 [uid]
        return line.replaceAll("/data/user/\\d+/", "/data/user/[uid]/")
                   .replaceAll("u0_a\\d+", "u0_a[uid]");
    }
}
