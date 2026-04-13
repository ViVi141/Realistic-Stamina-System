//! 环境天文/辐射计算工具（从 SCR_EnvironmentFactor.c 拆分）
class SCR_EnvironmentAstronomyMath
{
    static const float NATURAL_E = 2.718281828459045;

    static int DayOfYear(int year, int month, int day)
    {
        int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        bool isLeap = ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
        if (isLeap)
            mdays[1] = 29;

        int n = 0;
        int i = 0;
        while (i < month - 1)
        {
            n += mdays[i];
            i = i + 1;
        }
        n = n + day;
        return n;
    }

    static float SolarDeclination(int n)
    {
        return 23.44 * Math.DEG2RAD * Math.Sin(2.0 * Math.PI * (284.0 + n) / 365.0);
    }

    static float SolarCosZenith(float latDeg, int n, float localHour)
    {
        float latRad = latDeg * Math.DEG2RAD;
        float decl = SolarDeclination(n);
        float hourAngleDeg = 15.0 * (localHour - 12.0);
        float hourAngleRad = hourAngleDeg * Math.DEG2RAD;
        float cosTheta = Math.Sin(latRad) * Math.Sin(decl) + Math.Cos(latRad) * Math.Cos(decl) * Math.Cos(hourAngleRad);
        return cosTheta;
    }

    static float AirMass(float cosTheta)
    {
        if (cosTheta <= 0.0)
            return 9999.0;

        float thetaDeg = Math.Acos(cosTheta) * Math.RAD2DEG;
        float m = 1.0 / (cosTheta + 0.50572 * Math.Pow(96.07995 - thetaDeg, -1.6364));
        return m;
    }

    static float ClearSkyTransmittance(float airMass, float aerosolOpticalDepth)
    {
        float tau = Math.Pow(NATURAL_E, -aerosolOpticalDepth * airMass);
        return Math.Clamp(tau, 0.0, 1.0);
    }

    static float EstimateSeasonalAvgTemp(float lat, int dayOfYear)
    {
        float absLat = Math.AbsFloat(lat);
        float baseTemp = 27.0 - 0.4 * absLat;
        float seasonOffset = -10.0 * Math.Cos(2.0 * Math.PI * (float)(dayOfYear + 10) / 365.0);
        if (absLat > 40.0 && absLat < 60.0)
            baseTemp -= 3.0;
        return baseTemp + seasonOffset;
    }
}
