
package com.on00dev.apexcombatmod;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Layout simples criado programaticamente
        Button btnStart = new Button(this);
        btnStart.setText("INICIAR MOD MENU");
        btnStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startModMenu();
            }
        });
        
        setContentView(btnStart);
    }

    private void startModMenu() {
        // Verifica permissão de Overlay
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && !Settings.canDrawOverlays(this)) {
            Toast.makeText(this, "Permita a sobreposição!", Toast.LENGTH_LONG).show();
            Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
            startActivityForResult(intent, 123);
        } else {
            // Inicia o serviço
            startService(new Intent(this, FloatingModMenuService.class));
            Toast.makeText(this, "Mod Menu Iniciado!", Toast.LENGTH_SHORT).show();
        }
    }
}
