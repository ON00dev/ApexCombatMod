
package com.on00dev.apexcombatmod;

public class Native {
    // Carrega a biblioteca nativa 'modmenu' (ou 'apexcombatmod' dependendo do CMakeLists.txt)
    static {
        System.loadLibrary("apexcombatmod"); 
    }

    public static native void InitMod();
    public static native void SetGodModeOnline(boolean isEnabled);
    public static native void SetAutoDodge(boolean isEnabled);
    public static native void SetMissileFOV(boolean isEnabled);
    public static native void SetInfiniteEnergy(boolean isEnabled);
    public static native void SetPlaneSpeedHackEnabled(boolean isEnabled);
    public static native void SetPlaneSpeedMultiplier(int multiplier);
}
