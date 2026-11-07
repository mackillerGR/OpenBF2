package com.mackillergr.bf2;

/** Command-line geometry regression checks; runs without Android SDK or emulator. */
public final class MenuPointerMapperTest {
    private static void close(float actual, float expected) {
        if (Math.abs(actual - expected) > 0.06f)
            throw new AssertionError("actual=" + actual + " expected=" + expected);
    }
    private static void check(String name, float x, float y, int vw, int vh,
                              int gw, int gh, float targetX, float targetY) {
        float[] actual = MenuPointerMapper.map(x,y,vw,vh,gw,gh);
        close(actual[0],targetX);
        close(actual[1],targetY);
        System.out.println("PASS " + name);
    }
    public static void main(String[] args) {
        check("native center",1170,540,2340,1080,2340,1080,1170,540);
        check("native edge",0,0,2340,1080,2340,1080,0,0);
        check("native far edge",2339,1079,2340,1080,2340,1080,2339,1079);
        check("letterbox center",1170,540,2340,1080,1920,1080,1170,540);
        check("letterbox left",210,0,2340,1080,1920,1080,0,0);
        check("letterbox before left",0,0,2340,1080,1920,1080,0,0);
        check("portrait input center",540,1170,1080,2340,720,1280,540,1170);
        System.out.println("All Stage3.7 viewport/touch mapping tests passed.");
    }
}
