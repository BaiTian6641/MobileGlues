package com.fcl.plugin.mobileglues.settings

import com.google.gson.JsonObject
import com.google.gson.JsonParser
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class Fsr3ConfigTest {
    private fun parse(json: String): JsonObject = JsonParser.parseString(json).asJsonObject

    @Test
    fun `mode and quality values match the native settings contract`() {
        assertEquals(
            listOf("Disabled" to 0, "Experimental" to 1),
            Fsr3Mode.entries.map { it.name to it.wire },
        )
        assertEquals(
            listOf(
                "NativeAA" to 0, "Quality" to 1, "Balanced" to 2,
                "Performance" to 3, "UltraPerformance" to 4,
            ),
            Fsr3Quality.entries.map { it.name to it.wire },
        )
    }

    @Test
    fun `both modes round trip every quality without resetting the selection`() {
        Fsr3Mode.entries.forEach { mode ->
            Fsr3Quality.entries.forEach { quality ->
                val config = MGConfig.Default.copy(fsr3 = mode, fsr3Quality = quality)
                val encoded = MGConfigCodec.encode(config, null)

                assertEquals(mode.wire, encoded.get("fsr3Setting").asInt)
                assertEquals(quality.wire, encoded.get("fsr3Quality").asInt)
                assertTrue(encoded.get("fsr3Setting").asJsonPrimitive.isNumber)
                assertTrue(encoded.get("fsr3Quality").asJsonPrimitive.isNumber)
                val decoded = MGConfigCodec.decode(parse(encoded.toString()))
                assertEquals(config, decoded)
                assertEquals(mode == Fsr3Mode.Experimental, decoded.fsr3Enabled)
            }
        }
    }

    @Test
    fun `old configs default to disabled NativeAA and retain their FSR1 preset`() {
        val decoded = MGConfigCodec.decode(parse("""{"fsr1Setting":3,"maxGlslCacheSize":128}"""))

        assertEquals(Fsr3Mode.Disabled, decoded.fsr3)
        assertEquals(Fsr3Quality.NativeAA, decoded.fsr3Quality)
        assertFalse(decoded.fsr3Enabled)
        assertEquals(Fsr1Preset.Balanced, decoded.fsr1)
        assertEquals(GlslCacheSize.Limited(128), decoded.glslCache)
        val encoded = MGConfigCodec.encode(decoded, null)
        assertEquals(0, encoded.get("fsr3Setting").asInt)
        assertEquals(0, encoded.get("fsr3Quality").asInt)
        assertEquals(3, encoded.get("fsr1Setting").asInt)
    }

    @Test
    fun `invalid mode and quality fields fall back independently`() {
        listOf("-1", "2", "99", "null", "true", "{}", "[]", "\"unknown\"").forEach { bad ->
            val decoded = MGConfigCodec.decode(parse("""{"fsr3Setting":$bad,"fsr3Quality":2}"""))
            assertEquals("mode=$bad", Fsr3Mode.Disabled, decoded.fsr3)
            assertEquals("mode=$bad", Fsr3Quality.Balanced, decoded.fsr3Quality)
        }
        listOf("-1", "5", "99", "null", "true", "{}", "[]", "\"unknown\"").forEach { bad ->
            val decoded = MGConfigCodec.decode(parse("""{"fsr3Setting":1,"fsr3Quality":$bad}"""))
            assertEquals("quality=$bad", Fsr3Mode.Experimental, decoded.fsr3)
            assertEquals("quality=$bad", Fsr3Quality.NativeAA, decoded.fsr3Quality)
        }
    }

    @Test
    fun `numeric strings normalize to native integer values`() {
        val decoded = MGConfigCodec.decode(parse("""{"fsr3Setting":"1","fsr3Quality":" 4 "}"""))
        assertEquals(Fsr3Mode.Experimental, decoded.fsr3)
        assertEquals(Fsr3Quality.UltraPerformance, decoded.fsr3Quality)

        val encoded = MGConfigCodec.encode(decoded, null)
        assertTrue(encoded.get("fsr3Setting").asJsonPrimitive.isNumber)
        assertTrue(encoded.get("fsr3Quality").asJsonPrimitive.isNumber)
        assertEquals(1, encoded.get("fsr3Setting").asInt)
        assertEquals(4, encoded.get("fsr3Quality").asInt)
    }

    @Test
    fun `FSR3 edits own their keys while preserving foreign data and selected quality`() {
        val root = parse(
            """{"fsr3Setting":1,"fsr3Quality":3,"hideMGEnvLevel":2,"future":{"flags":[true,7],"keep":true}}"""
        )
        val original = root.deepCopy()
        val foreign = MGConfigCodec.foreignKeysOf(root)
        assertNull(foreign.get("fsr3Setting"))
        assertNull(foreign.get("fsr3Quality"))

        val disabled = MGConfigCodec.decode(root).copy(fsr3 = Fsr3Mode.Disabled)
        val encoded = MGConfigCodec.encode(disabled, foreign)
        assertEquals(0, encoded.get("fsr3Setting").asInt)
        assertEquals(3, encoded.get("fsr3Quality").asInt)
        assertEquals(2, encoded.get("hideMGEnvLevel").asInt)
        assertEquals(original.get("future"), encoded.get("future"))
        assertEquals(disabled, MGConfigCodec.decode(encoded))

        encoded.getAsJsonObject("future").addProperty("keep", false)
        assertEquals(original, root)
        assertEquals(original.get("future"), foreign.get("future"))
    }
}
