//! 通用气温采样触发（从 SCR_RSS_EnvironmentFactor.c 拆分）

class SCR_RSS_TemperatureSampler
{
    //! 按时间/位移触发重算；返回是否更新了温度
    static bool MaybeUpdateUniversalTemperature(
        float currentTime,
        float tempUpdateInterval,
        float lat,
        int dayOfYear,
        float tod,
        float altM,
        float cloud,
        float rain,
        float fogDensity,
        IEntity owner,
        float recalcDistanceSq,
        inout float cachedSurfaceTemperature,
        inout float lastTemperatureUpdateTime,
        inout float nextTempStepLogTime,
        inout vector lastTempCalcPosition,
        inout bool tempPositionInitialized)
    {
        bool updated = false;

        if (lastTemperatureUpdateTime <= 0.0 && cachedSurfaceTemperature == 20.0)
        {
            cachedSurfaceTemperature = SCR_RSS_AstronomyMath.CalculateUniversalTemperature(
                lat, dayOfYear, tod, altM, cloud, rain, fogDensity);
            lastTemperatureUpdateTime = currentTime;
            if (owner)
            {
                lastTempCalcPosition = owner.GetOrigin();
                tempPositionInitialized = true;
            }
            updated = true;
        }

        bool timeTrigger = (currentTime - lastTemperatureUpdateTime >= tempUpdateInterval);
        bool posTrigger = false;
        if (owner && tempPositionInitialized)
        {
            vector curPos = owner.GetOrigin();
            float dSq = vector.DistanceSq(curPos, lastTempCalcPosition);
            posTrigger = (dSq >= recalcDistanceSq);
        }

        if (timeTrigger || posTrigger)
        {
            cachedSurfaceTemperature = SCR_RSS_AstronomyMath.CalculateUniversalTemperature(
                lat, dayOfYear, tod, altM, cloud, rain, fogDensity);
            lastTemperatureUpdateTime = currentTime;
            nextTempStepLogTime = currentTime + tempUpdateInterval;
            if (owner)
                lastTempCalcPosition = owner.GetOrigin();
            updated = true;
        }

        return updated;
    }
}
