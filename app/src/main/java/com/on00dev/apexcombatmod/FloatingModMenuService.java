package com.on00dev.apexcombatmod;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.IBinder;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

public class FloatingModMenuService extends Service {
    private WindowManager windowManager;
    private FrameLayout rootLayout;
    private LinearLayout menuLayout;
    private ImageView iconView;
    private WindowManager.LayoutParams params;

    @Override
    public void onCreate() {
        super.onCreate();
        
        // Inicializa a lib nativa
        try {
            Native.InitMod();
        } catch (Exception e) {
            // Silencioso
        }

        windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);

        // Configuração da Janela
        int layoutType;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            layoutType = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
        } else {
            layoutType = WindowManager.LayoutParams.TYPE_PHONE;
        }

        params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                layoutType,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT);

        params.gravity = Gravity.TOP | Gravity.START;
        params.x = 0;
        params.y = 100;

        // Container Principal
        rootLayout = new FrameLayout(this);
        
        // --- 1. ÍCONE FLUTUANTE (Minimizado) ---
        iconView = new ImageView(this);
        iconView.setImageResource(android.R.drawable.ic_menu_manage); // Ícone de engrenagem
        
        // Estilo do ícone (Fundo Circular)
        GradientDrawable iconBg = new GradientDrawable();
        iconBg.setShape(GradientDrawable.OVAL);
        iconBg.setColor(Color.parseColor("#AA000000")); // Preto translúcido
        iconBg.setStroke(2, Color.parseColor("#e28000ff")); // Borda vermelha
        iconView.setBackground(iconBg);
        
        int iconSize = dpToPx(50);
        FrameLayout.LayoutParams iconParams = new FrameLayout.LayoutParams(iconSize, iconSize);
        iconView.setLayoutParams(iconParams);
        iconView.setPadding(dpToPx(10), dpToPx(10), dpToPx(10), dpToPx(10));
        
        // --- 2. MENU EXPANDIDO ---
        menuLayout = new LinearLayout(this);
        menuLayout.setOrientation(LinearLayout.VERTICAL);
        
        // Estilo do Menu (Cantos ArparseColor("#e28000ff")ondados)
        GradientDrawable menuBg = new GradientDrawable();
        menuBg.setColor(Color.parseColor("#EE1C1C1C")); // Cinza escuro quase opaco
        menuBg.setCornerRadius(dpToPx(15));
        menuBg.setStroke(2, Color.parseColor("#e28000ff"));
        menuLayout.setBackground(menuBg);
        
        menuLayout.setPadding(dpToPx(20), dpToPx(20), dpToPx(20), dpToPx(20));
        menuLayout.setVisibility(View.GONE); // Começa minimizado

        // Cabeçalho do Menu (Título + Botão Minimizar)
        LinearLayout headerLayout = new LinearLayout(this);
        headerLayout.setOrientation(LinearLayout.HORIZONTAL);
        headerLayout.setGravity(Gravity.CENTER_VERTICAL);
        
        TextView title = new TextView(this);
        title.setText("ON00dev | Apex Combat Mod");
        title.setTextColor(Color.parseColor("#e28000ff"));
        title.setTextSize(18);
        title.setTypeface(null, android.graphics.Typeface.BOLD);
        LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.WRAP_CONTENT, 1.0f);
        title.setLayoutParams(titleParams);
        headerLayout.addView(title);

        // Botão Minimizar [-]
        TextView minimizeBtn = new TextView(this);
        minimizeBtn.setText("—"); // Travessão ou sinal de menos
        minimizeBtn.setTextColor(Color.WHITE);
        minimizeBtn.setTextSize(24);
        minimizeBtn.setPadding(dpToPx(10), 0, dpToPx(10), 0);
        minimizeBtn.setOnClickListener(v -> {
            menuLayout.setVisibility(View.GONE);
            iconView.setVisibility(View.VISIBLE);
        });
        headerLayout.addView(minimizeBtn);
        
        menuLayout.addView(headerLayout);
        
        // Linha divisória
        View divider = new View(this);
        divider.setBackgroundColor(Color.DKGRAY);
        LinearLayout.LayoutParams dividerParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dpToPx(1));
        dividerParams.setMargins(0, dpToPx(5), 0, dpToPx(10));
        menuLayout.addView(divider, dividerParams);

        // Switch: Offline
        Switch swGodMode = createSwitch("God Mode (Offline/PvE)");
        swGodMode.setOnCheckedChangeListener((buttonView, isChecked) -> Native.SetGodModeOnline(isChecked));
        menuLayout.addView(swGodMode);

        // Switch: Energia Infinita (Manobras)
        Switch swEnergy = createSwitch("Energia Infinita");
        swEnergy.setOnCheckedChangeListener((buttonView, isChecked) -> Native.SetInfiniteEnergy(isChecked));
        menuLayout.addView(swEnergy);

        final int[] planeSpeedMultiplier = {1};

        // Switch: Velocidade da Aeronave
        Switch swPlaneSpeed = createSwitch("Velocidade do Aviao");
        swPlaneSpeed.setOnCheckedChangeListener((buttonView, isChecked) -> {
            Native.SetPlaneSpeedMultiplier(planeSpeedMultiplier[0]);
            Native.SetPlaneSpeedHackEnabled(isChecked);
        });
        menuLayout.addView(swPlaneSpeed);

        TextView tvPlaneSpeedMultiplier = new TextView(this);
        tvPlaneSpeedMultiplier.setTextColor(Color.WHITE);
        tvPlaneSpeedMultiplier.setText("Multiplicador: " + planeSpeedMultiplier[0] + "x");
        tvPlaneSpeedMultiplier.setPadding(0, 0, 0, dpToPx(4));
        menuLayout.addView(tvPlaneSpeedMultiplier);

        SeekBar sbPlaneSpeed = new SeekBar(this);
        sbPlaneSpeed.setMax(9);
        sbPlaneSpeed.setProgress(planeSpeedMultiplier[0] - 1);
        sbPlaneSpeed.setPadding(0, 0, 0, dpToPx(8));
        sbPlaneSpeed.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                planeSpeedMultiplier[0] = progress + 1;
                tvPlaneSpeedMultiplier.setText("Multiplicador: " + planeSpeedMultiplier[0] + "x");
                Native.SetPlaneSpeedMultiplier(planeSpeedMultiplier[0]);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });
        menuLayout.addView(sbPlaneSpeed);

        Native.SetPlaneSpeedMultiplier(planeSpeedMultiplier[0]);

        // Switch: Hit Kill
        Switch swHitKill = createSwitch("Hit Kill (Rapid Fire)");
        swHitKill.setOnCheckedChangeListener((buttonView, isChecked) -> Native.SetGodModeOnline(isChecked));
        menuLayout.addView(swHitKill);

        // Switch: Auto Dodge
        Switch swAutoDodge = createSwitch("Auto Dodge (God Mode)");
        swAutoDodge.setOnCheckedChangeListener((buttonView, isChecked) -> Native.SetAutoDodge(isChecked));
        menuLayout.addView(swAutoDodge);
        
        // Switch: Missile FOV (Magic Bullet)
        Switch swMissileFov = createSwitch("Missíl FOV (Magic Bullet)");
        swMissileFov.setOnCheckedChangeListener((buttonView, isChecked) -> Native.SetMissileFOV(isChecked));
        menuLayout.addView(swMissileFov);

        // Botão Fechar Serviço
        TextView closeBtn = new TextView(this);
        closeBtn.setText("FECHAR MOD MENU");
        closeBtn.setTextColor(Color.parseColor("#e28000ff"));
        closeBtn.setGravity(Gravity.CENTER);
        closeBtn.setPadding(0, dpToPx(15), 0, 0);
        closeBtn.setTypeface(null, android.graphics.Typeface.BOLD);
        closeBtn.setOnClickListener(v -> stopSelf());
        menuLayout.addView(closeBtn);

        // Adiciona Views ao Root
        rootLayout.addView(iconView);
        rootLayout.addView(menuLayout);

        // Adiciona à tela
        try {
            windowManager.addView(rootLayout, params);
        } catch (Exception e) {
            e.printStackTrace();
        }
        
        // --- Lógica de Arrastar e Clique ---
        View.OnTouchListener dragListener = new View.OnTouchListener() {
            private int initialX;
            private int initialY;
            private float initialTouchX;
            private float initialTouchY;
            private boolean isClick;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = params.x;
                        initialY = params.y;
                        initialTouchX = event.getRawX();
                        initialTouchY = event.getRawY();
                        isClick = true;
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        int dx = (int) (event.getRawX() - initialTouchX);
                        int dy = (int) (event.getRawY() - initialTouchY);
                        
                        // Se moveu mais que um pouco, não é clique
                        if (Math.abs(dx) > 10 || Math.abs(dy) > 10) {
                            isClick = false;
                        }

                        params.x = initialX + dx;
                        params.y = initialY + dy;
                        windowManager.updateViewLayout(rootLayout, params);
                        return true;
                    case MotionEvent.ACTION_UP:
                        if (isClick && v == iconView) {
                            // Clicou no ícone: Abre menu
                            iconView.setVisibility(View.GONE);
                            menuLayout.setVisibility(View.VISIBLE);
                        }
                        return true;
                }
                return false;
            }
        };

        // Aplica o listener ao ícone e ao header do menu
        iconView.setOnTouchListener(dragListener);
        headerLayout.setOnTouchListener(dragListener); 
    }
    
    private Switch createSwitch(String text) {
        Switch sw = new Switch(this);
        sw.setText(text);
        sw.setTextColor(Color.WHITE);
        sw.setPadding(0, dpToPx(5), 0, dpToPx(5));
        return sw;
    }

    private int dpToPx(int dp) {
        return (int) (dp * getResources().getDisplayMetrics().density);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (rootLayout != null) windowManager.removeView(rootLayout);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
    
    private void startForegroundService() {
        String channelId = "mod_menu_channel";
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    channelId,
                    "Mod Menu Service",
                    NotificationManager.IMPORTANCE_LOW
            );
            getSystemService(NotificationManager.class).createNotificationChannel(channel);
        }

        Notification notification = new Notification.Builder(this, channelId)
                .setContentTitle("Apex Combat Mod")
                .setContentText("Overlay Ativo")
                .setSmallIcon(android.R.drawable.ic_menu_manage)
                .build();

        startForeground(1, notification);
    }
}
