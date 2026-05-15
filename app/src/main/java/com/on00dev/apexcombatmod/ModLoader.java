package com.on00dev.apexcombatmod;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;
import android.os.Environment;
import android.provider.Settings;
import android.widget.Toast;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.Properties;

public class ModLoader {
    private static final String TAG = "ApexCombatMod";
    private static final String PREFS_NAME = "ModTrialPrefs";
    
    // 10080 minutos = 10080 * 60 * 1000 = 604800000 ms
    private static final long TRIAL_DURATION_MS = 604800000; 
    private static final long UPDATE_INTERVAL_MS = 60000; // Atualiza a cada 1 minuto
    
    private static Handler handler;
    private static Runnable timerRunnable;
    private static String deviceId = "unknown_device";

    public static void load(Context context) {
        // Pega o Android ID para identificar o aparelho unicamente
        deviceId = Settings.Secure.getString(context.getContentResolver(), Settings.Secure.ANDROID_ID);
        if (deviceId == null) deviceId = "unknown_device";

        // Migra dados do armazenamento externo para SharedPreferences (caso tenha reinstalado)
        syncTimeWithExternalStorage(context);

        if (isTrialValid(context)) {
            try {
                System.loadLibrary("apexcombatmod");
                
                Intent intent = new Intent();
                intent.setClassName(context, "com.on00dev.apexcombatmod.FloatingModMenuService");
                context.startService(intent);
                
                startTrialTimer(context);
                
            } catch (UnsatisfiedLinkError e) {
                // Silencioso
            } catch (Exception e) {
                // Silencioso
            }
        } else {
            // Silencioso
        }
    }

    private static String getKey() {
        return "accumulated_time_" + deviceId;
    }

    private static File getExternalTrialFile() {
        // Salva em uma pasta pública que sobrevive a desinstalações (ex: pasta Download)
        // Usamos um nome de arquivo disfarçado para não chamar atenção
        File dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        return new File(dir, ".sys_cache_acm_" + deviceId + ".dat");
    }

    private static void syncTimeWithExternalStorage(Context context) {
        long localTime = getLocalAccumulatedTime(context);
        long externalTime = getExternalAccumulatedTime();
        
        // Mantém sempre o maior tempo (para evitar reset por reinstalação ou limpeza de dados)
        long maxTime = Math.max(localTime, externalTime);
        
        if (maxTime > localTime) {
            saveLocalAccumulatedTime(context, maxTime);
        }
        if (maxTime > externalTime) {
            saveExternalAccumulatedTime(maxTime);
        }
    }

    private static long getLocalAccumulatedTime(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        return prefs.getLong(getKey(), 0);
    }

    private static void saveLocalAccumulatedTime(Context context, long timeMs) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().putLong(getKey(), timeMs).apply();
    }

    private static long getExternalAccumulatedTime() {
        File file = getExternalTrialFile();
        if (!file.exists()) return 0;
        try (FileInputStream fis = new FileInputStream(file)) {
            Properties props = new Properties();
            props.load(fis);
            String val = props.getProperty("t");
            if (val != null) return Long.parseLong(val);
        } catch (Exception e) {
            // Ignora erro
        }
        return 0;
    }

    private static void saveExternalAccumulatedTime(long timeMs) {
        File file = getExternalTrialFile();
        try {
            if (!file.exists()) {
                file.getParentFile().mkdirs();
                file.createNewFile();
            }
            Properties props = new Properties();
            props.setProperty("t", String.valueOf(timeMs));
            try (FileOutputStream fos = new FileOutputStream(file)) {
                props.store(fos, null);
            }
        } catch (Exception e) {
            // Ignora erro
        }
    }

    private static long getAccumulatedTime(Context context) {
        return Math.max(getLocalAccumulatedTime(context), getExternalAccumulatedTime());
    }

    private static void saveAccumulatedTime(Context context, long timeMs) {
        saveLocalAccumulatedTime(context, timeMs);
        saveExternalAccumulatedTime(timeMs);
    }

    private static boolean isTrialValid(Context context) {
        return getAccumulatedTime(context) < TRIAL_DURATION_MS;
    }
    
    private static void startTrialTimer(final Context context) {
        if (handler == null) {
            handler = new Handler(Looper.getMainLooper());
        }
        
        timerRunnable = new Runnable() {
            @Override
            public void run() {
                long currentAccumulated = getAccumulatedTime(context);
                long newAccumulated = currentAccumulated + UPDATE_INTERVAL_MS;
                saveAccumulatedTime(context, newAccumulated);
                
                if (newAccumulated >= TRIAL_DURATION_MS) {
                    // Silencioso, apenas paramos de atualizar e injetar o menu na próxima vez.
                } else {
                    handler.postDelayed(this, UPDATE_INTERVAL_MS);
                }
            }
        };
        
        handler.postDelayed(timerRunnable, UPDATE_INTERVAL_MS);
    }
}
