<?xml version="1.0" encoding="UTF-8"?>

<!--
Copyright (c) 2005, Daniel C. Newman <dan.newman@mtbaldy.us>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 + Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

 + Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.

 + Neither the name of mtbaldy.us nor the names of its contributors
   may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
-->

<xsl:stylesheet version="1.0"
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<!--            xmlns:str="http://exslt.org/strings"
                exclude-result-prefixes="str"> -->

  <!-- Global parameters are after the global variables: some of their
       their default values depends upon the values of the global variables -->

  <xsl:include href="xml_const.xsl"/>

  <!-- Compass points: we here assume that each point can be represented
       by a *single* character.  We need to know these codes for purposes
       of converting wind directions from "blowing from" to "blowing to"
       and vice versa. -->
  <xsl:variable name="compass-N" select="'N'"/>  <!-- North -->
  <xsl:variable name="compass-S" select="'S'"/>  <!-- South -->
  <xsl:variable name="compass-E" select="'E'"/>  <!-- East  -->
  <xsl:variable name="compass-W" select="'W'"/>  <!-- West  -->

  <xsl:variable name="wind-dirs" select="concat($compass-N,$compass-S,
					 $compass-E,$compass-W)"/>
  <xsl:variable name="wind-flip" select="concat($compass-S,$compass-N,
					 $compass-W,$compass-E)"/>

  <!-- Background colors -->
  <xsl:variable name="th1_bgcolor" select="'#00ddff'"/>
  <xsl:variable name="th2_bgcolor" select="'#88eeff'"/>
  <xsl:variable name="td_bgcolor"  select="'#ccffff'"/>
  <xsl:variable name="txt_color"   select="'#000066'"/>
  <xsl:variable name="bg_color"    select="'#eeffff'"/>

  <!-- Devices undergoing calibration -->
  <xsl:variable name="calibrating" select="''"/>

  <!-- Parameters adjustable via the invocation command -->
  <xsl:param name="dewp-report"  select="$u-F"/>
  <xsl:param name="dewp-display" select="'Dew point'"/>
  <xsl:param name="leng-report"  select="$u-ft"/>
  <xsl:param name="leng-display" select="$u-ft"/>
  <xsl:param name="temp-report"  select="$u-F"/>
  <xsl:param name="temp-display" select="concat('&#176;',$u-F)"/>
  <xsl:param name="rh-display"   select="' rel. humidity'"/>
  <xsl:param name="pres-report"  select="$u-inHg"/>
  <xsl:param name="pres-display" select="'&quot;Hg'"/>
  <xsl:param name="rain-report"  select="$u-in"/>
  <xsl:param name="rain-display" select="'&quot;'"/>
  <xsl:param name="wdir-report"  select="$u-wf"/>
  <xsl:param name="wdir-display" select="$u-wf"/>
  <xsl:param name="wvel-report"  select="$u-mph"/>
  <xsl:param name="wvel-display" select="' MPH'"/>

  <xsl:param name="output-method" select="'html'"/>

  <!-- Display the output units associated with a given measurement type -->

  <xsl:template name="format-units">
    <xsl:param name="mt"/>               <!-- Measurement type -->
    <xsl:param name="full" select="0"/>  <!-- When > 0, display long name -->
    <xsl:choose>
      <xsl:when test="$mt = $leng">
	<xsl:value-of select="$leng-display"/>
      </xsl:when>
      <xsl:when test="($mt = $temp) or ($mt = $dewp)">
	<xsl:value-of select="$temp-display"/>
      </xsl:when>
      <xsl:when test="$mt = $rh">
	<xsl:value-of select="$u-rh"/>
	<xsl:if test="$full &gt; 0">
	  <xsl:value-of select="$rh-display"/>
	</xsl:if>
      </xsl:when>
      <xsl:when test="($mt = $pres) or ($mt = $prsl) or ($mt = $prsl12) or
	              ($mt = $prsl0)">
	<xsl:value-of select="$pres-display"/>
      </xsl:when>
    </xsl:choose>
  </xsl:template>


  <!-- Convert a length measurement to units of meters.  Permitted input
       units are meters (m), millimeters (mm), centimeters (cm), kilometers
       (km), feet (ft), miles (mi), and inches (in). -->

  <xsl:template name="to-m">
    <xsl:param name="v" select="0"/>  <!-- Input value to convert -->
    <xsl:param name="u" select="m"/>  <!-- Units of the input value -->
    <xsl:choose>
      <xsl:when test="$u = $u-m">
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:when test="$u = $u-mm">
	<xsl:value-of select="number($v) div 1000"/>
      </xsl:when>
      <xsl:when test="$u = $u-cm">
	<xsl:value-of select="number($v) div 100"/>
      </xsl:when>
      <xsl:when test="$u = $u-km">
	<xsl:value-of select="number($v) * 1000"/>
      </xsl:when>
      <xsl:when test="$u = $u-ft">
	<xsl:value-of select="number($v) * 0.3048"/>
      </xsl:when>
      <xsl:when test="$u = $u-mi">
	<xsl:value-of select="number($v) * 1609.344"/>
      </xsl:when>
      <xsl:when test="$u = $u-in">
	<xsl:value-of select="number($v) * 0.0254"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="0"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Convert a length measurement from units of meters.  Permitted output
       units are meters (m), millimeters (mm), centimeters (cm), kilometers
       (km), feet (ft), miles (mi), and inches (in). -->

  <xsl:template name="from-m">
    <xsl:param name="v" select="0"/>  <!-- Input value to convert -->
    <xsl:param name="u" select="m"/>  <!-- Units of the output value -->
    <xsl:choose>
      <xsl:when test="$u = $u-m">
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:when test="$u = $u-mm">
	<xsl:value-of select="number($v) * 1000"/>
      </xsl:when>
      <xsl:when test="$u = $u-cm">
	<xsl:value-of select="number($v) * 100"/>
      </xsl:when>
      <xsl:when test="$u = $u-km">
	<xsl:value-of select="number($v) div 1000"/>
      </xsl:when>
      <xsl:when test="$u = $u-ft">
	<xsl:value-of select="number($v) div 0.3048"/>
      </xsl:when>
      <xsl:when test="$u = $u-mi">
	<xsl:value-of select="number($v) div 1609.344"/>
      </xsl:when>
      <xsl:when test="$u = $u-in">
	<xsl:value-of select="number($v) div 0.0254"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="0"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Convert a length v from units of u-in to units of u-out -->

  <xsl:template name="convert-leng">
    <xsl:param name="v" select="0"/>  <!-- Input length -->
    <xsl:param name="u-in"/>          <!-- Units of the input length -->
    <xsl:param name="u-out"/>         <!-- Units of the converted length -->
    <!-- We have an N x N problem here: N input units and N output units
	 Lets convert to a cannonical unit and then output from there.
	 We'll convert to meters and from there to u-out. -->
    <xsl:choose>
      <xsl:when test="$u-in = $u-out">
	<!--  No conversion necessary: input and output units are the same -->
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:call-template name="from-m">
	  <xsl:with-param name="u" select="$u-out"/>
	  <xsl:with-param name="v">
	    <xsl:call-template name="to-m">
	      <xsl:with-param name="u" select="$u-in"/>
	      <xsl:with-param name="v" select="$v"/>
	    </xsl:call-template>
	  </xsl:with-param>
	</xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Convert a time measurement to units of seconds.  Permitted input
       units are seconds (s), minutes (m), hours (h), and days (d). -->

  <xsl:template name="to-s">
    <xsl:param name="v" select="0"/>  <!-- Input value to convert -->
    <xsl:param name="u" select="s"/>  <!-- Units of the input value -->
    <xsl:choose>
      <xsl:when test="$u = $u-s">
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:when test="$u = $u-min">
	<xsl:value-of select="number($v) * 60"/>
      </xsl:when>
      <xsl:when test="$u = $u-hr">
	<xsl:value-of select="number($v) * 3600"/>
      </xsl:when>
      <xsl:when test="$u = $u-d">
	<xsl:value-of select="number($v) * 86400"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="0"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Convert a temperature to from Celsius (C), Fahrenheit (F), or
       Kelvin (K) to Celsius / Centigrade -->

  <xsl:template name="to-C">
    <xsl:param name="v" select="0"/>     <!-- Temperature to convert -->
    <xsl:param name="u" select="$u-C"/>  <!-- Temperature units of v -->
    <xsl:choose>
      <xsl:when test="$u = $u-C">
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:when test="$u = $u-K">
	<xsl:value-of select="number($v) - 273.15"/>
      </xsl:when>
      <xsl:when test="$u = $u-F">
	<xsl:value-of select="(number($v) - 32) * (5 div 9)"/>
      </xsl:when>
    </xsl:choose>
  </xsl:template>


  <!-- Convert a temperature from Celsius to either Celsius (C),
       Fahrenheit (F), or Kelvin (K) -->

  <xsl:template name="from-C">
    <xsl:param name="v" select="0"/>     <!-- Temperature to convert -->
    <xsl:param name="u" select="$u-C"/>  <!-- Units to convert to    -->
    <xsl:choose>
      <xsl:when test="$u = $u-C">
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:when test="$u = $u-K">
	<xsl:value-of select="number($v) + 273.15"/>
      </xsl:when>
      <xsl:when test="$u = $u-F">
	<xsl:value-of select="number($v) * (9 div 5) + 32"/>
      </xsl:when>
    </xsl:choose>
  </xsl:template>


  <!-- Convert a temperature v from units of u-in to units of u-out -->

  <xsl:template name="convert-temp">
    <xsl:param name="v" select="0"/>  <!-- Input temperature -->
    <xsl:param name="u-in"/>          <!-- Units of the input temperature -->
    <xsl:param name="u-out"/>         <!-- Units of the converted temp -->
    <!-- We have an N x N problem here: N input units and N output units
	 Lets convert to a cannonical unit and then output from there.
	 We'll convert to Celsius/Centigrade and from there to u-out. -->
    <xsl:choose>
      <xsl:when test="$u-in = $u-out">
	<!--  No conversion necessary: input and output units are the same -->
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:call-template name="from-C">
	  <xsl:with-param name="u" select="$u-out"/>
	  <xsl:with-param name="v">
	    <xsl:call-template name="to-C">
	      <xsl:with-param name="u" select="$u-in"/>
	      <xsl:with-param name="v" select="$v"/>
	    </xsl:call-template>
	  </xsl:with-param>
	</xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Convert a pressure to units of atmospheres -->

  <xsl:template name="to-atm">
    <xsl:param name="v"/>  <!-- Value to convert to atm -->
    <xsl:param name="u"/>  <!-- Units in which v is expressed -->
    <xsl:choose>
      <xsl:when test="$u = $u-atm">
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:when test="$u = $u-inHg">
	<xsl:value-of select="number($v) div (29 + (117 div 127))"/>
      </xsl:when>
      <xsl:when test="($u = $u-mmHg) or ($u = $u-Torr)">
	<!-- Note that a 1 Torr is not exactly 1 mmHg owing to a difference
	     in definition.  However, the deviation is very, very slight -->
	<xsl:value-of select="number($v) div 760"/>
      </xsl:when>
      <xsl:when test="($u = $u-hPa) or ($u = $u-mbar) or ($u = $u-mb)">
	<xsl:value-of select="number($v) div 1013.25"/>
      </xsl:when>
      <xsl:when test="$u = $u-kPa">
	<xsl:value-of select="number($v) div 101.325"/>
      </xsl:when>
      <xsl:when test="$u = $u-Pa">
	<xsl:value-of select="number($v) div 101325"/>
      </xsl:when>
      <xsl:when test="$u = $u-at">
	<xsl:value-of select="number($v) div (1 + (6517 div 196133))"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="0"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Convert a pressure from units of atmospheres -->

  <xsl:template name="from-atm">
    <xsl:param name="v"/>  <!-- Pressure value in units of atmospheres -->
    <xsl:param name="u"/>  <!-- Units to convert v to -->
    <xsl:choose>
      <xsl:when test="$u = $u-atm">
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:when test="$u = $u-inHg">
	<xsl:value-of select="format-number(number($v) * (29 + (117 div 127)),
			      '#.00')"/>
      </xsl:when>
      <xsl:when test="($u = $u-mmHg) or ($u = $u-Torr)">
	<!-- Note that a 1 Torr is not exactly 1 mmHg owing to a difference
	     in definition.  However, the deviation is very, very slight -->
	<xsl:value-of select="number($v) * 760"/>
      </xsl:when>
      <xsl:when test="($u = $u-hPa) or ($u = $u-mbar) or ($u = $u-mb)">
	<xsl:value-of select="number($v) * 1013.25"/>
      </xsl:when>
      <xsl:when test="$u = $u-kPa">
	<xsl:value-of select="number($v) * 101.325"/>
      </xsl:when>
      <xsl:when test="$u = $u-Pa">
	<xsl:value-of select="number($v) * 101325"/>
      </xsl:when>
      <xsl:when test="$u = $u-at">
	<xsl:value-of select="number($v) * (1 + (6517 div 196133))"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="0"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Convert pressure from one unit to another -->

  <xsl:template name="convert-pres">
    <xsl:param name="v" select="0"/>  <!-- Input pressure -->
    <xsl:param name="u-in"/>          <!-- Units of the input pressure -->
    <xsl:param name="u-out"/>         <!-- Units of the converted press -->
    <!-- We have an N x N problem here: N input units and N output units
	 Lets convert to a cannonical unit and then output from there.
	 We'll convert to atmospheres and from there to u-out. -->
    <xsl:choose>
      <xsl:when test="$u-in = $u-out">
	<!--  No conversion necessary: input and output units are the same -->
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:call-template name="from-atm">
	  <xsl:with-param name="u" select="$u-out"/>
	  <xsl:with-param name="v">
	    <xsl:call-template name="to-atm">
	      <xsl:with-param name="u" select="$u-in"/>
	      <xsl:with-param name="v" select="$v"/>
	    </xsl:call-template>
	  </xsl:with-param>
	</xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Convert wind directions -->

  <xsl:template name="convert-wind">
    <xsl:param name="v" select="''"/>
    <xsl:param name="u-in"/>
    <xsl:param name="u-out"/>
    <xsl:choose>
      <xsl:when test="$u-in = $u-out">
	<!-- Input and output units are identical -->
	<xsl:value-of select="$v"/>
      </xsl:when>
      <xsl:otherwise>
	<!--  All other conversions are just a 180 degree rotation -->
	<xsl:value-of select="translate($v,$wind-dirs,$wind-flip)"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Convert the input value v with units u to the desired display units
       as determined from the measurement type mt (e.g., "temp") and the
       corresponding global _report parameter (e.g., report-temp).
       The displayed value will show the units when du == 1 and a longer
       unit display when du == 2 (e.g., "%" vs. "% relative humidity"). -->

  <xsl:template name="format">
    <xsl:param name="v"/>  <!-- value to format -->
    <xsl:param name="mt"/> <!-- value type -->
    <xsl:param name="u"/>  <!-- units -->
    <xsl:param name="du"/> <!-- display units? -->
    <xsl:param name="t"/>  <!-- time -->
    <xsl:choose>
      <xsl:when test="$v">
	<xsl:choose>
	  <xsl:when test="$missing-value = translate($v,' ','')">
	    <!-- Missing value -->
	    <xsl:text>&#160;</xsl:text>
	  </xsl:when>
	  <xsl:when test="$mt = $leng">
	    <!-- Length -->
	    <xsl:variable name="v_report">
	      <xsl:call-template name="convert-leng">
		<xsl:with-param name="v"     select="$v"/>
		<xsl:with-param name="u-in"  select="$u"/>
		<xsl:with-param name="u-out" select="$leng-report"/>
	      </xsl:call-template>
	    </xsl:variable>
	    <xsl:value-of select="round($v_report)"/>
	  </xsl:when>
	  <xsl:when test="($mt = $temp) or ($mt = $dewp)">
	    <!-- Temperature -->
	    <xsl:variable name="v_report">
	      <xsl:call-template name="convert-temp">
		<xsl:with-param name="v"     select="$v"/>
		<xsl:with-param name="u-in"  select="$u"/>
		<xsl:with-param name="u-out" select="$temp-report"/>
	      </xsl:call-template>
	    </xsl:variable>
	    <xsl:value-of select="format-number($v_report, '#.0')"/>
	  </xsl:when>
	  <xsl:when test="$mt = $rh">
	    <!-- Relative humidity -->
	    <xsl:variable name="v_report" select="number($v)"/>
	    <xsl:choose>
	      <xsl:when test="$v_report &lt; 0">
		<xsl:text>0</xsl:text>
	      </xsl:when>
	      <xsl:when test="$v_report &gt; 100">
		<xsl:text>100</xsl:text>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:value-of select="round($v_report)"/>
	      </xsl:otherwise>
	    </xsl:choose>
	  </xsl:when>
	  <xsl:when test="($mt = $pres) or ($mt = $prsl) or ($mt = $prsl12)
	                                or ($mt = $prsl0)">
	    <!-- Atmospheric pressure -->
	    <xsl:variable name="v_report">
	      <xsl:call-template name="convert-pres">
		<xsl:with-param name="v"     select="$v"/>
		<xsl:with-param name="u-in"  select="$u"/>
		<xsl:with-param name="u-out" select="$pres-report"/>
	      </xsl:call-template>
	    </xsl:variable>
	    <xsl:value-of select="$v_report"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of select="$v"/>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:if test="$du &gt; 0">
	  <xsl:call-template name="format-units">
	    <xsl:with-param name="mt" select="$mt"/>
	    <xsl:with-param name="full" select="$du - 1"/>
	  </xsl:call-template>
	</xsl:if>
	<xsl:if test="$t">
	  <xsl:value-of select="concat(' (',$t,')')"/>
	</xsl:if>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>&#160;</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Based upon running average data, determine if the current value
       is decreasing, increasing, or remaining stable -->

  <xsl:template name="trend">
    <xsl:param name="v"/>     <!-- Current value -->
    <xsl:param name="avgs"/>  <!-- SP delimited list of running averages,
				   sorted from shortest to longest time
				   span -->
    <xsl:if test="$v and (string-length($avgs) &gt; 0)">
      <!-- Pick off the first average in the list.  It is slightly easier
	   to use str:tokenize($avgs, ' ') but only slightly.  -->
      <xsl:variable name="avg">
	<xsl:choose>
	  <xsl:when test="substring-before($avgs, ' ')">
	    <xsl:value-of select="substring-before($avgs, ' ')"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of select="$avgs"/>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:variable>
      <xsl:choose>
	<xsl:when test="number($v) &gt; number($avg)">
	  <!-- Current value exceeds running average -->
	  <xsl:text>&#8593;</xsl:text>
	</xsl:when>
	<xsl:when test="number($v) &lt; number($avg)">
	  <!-- Current value is less than the running average -->
	  <xsl:text>&#8595;</xsl:text>
	</xsl:when>
	<xsl:otherwise>
	  <!-- Current value is the same as this running average.  Let's
	       Redo the test using the next running average which has a
	       longer time span -->
	  <xsl:if test="substring-after($avgs, ' ')">
	    <xsl:call-template name="trend">
	      <xsl:with-param name="v" select="$v"/>
	      <xsl:with-param name="avgs" select="substring-after($avgs,' ')"/>
	    </xsl:call-template>
	  </xsl:if>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:template>


  <!-- Display a list of running averages.  So as to avoid requiring
       str:tokenize(), we do this recursively. -->

  <xsl:template name="averages">
    <xsl:param name="avgs"/>  <!-- SP delimited list of running averages,
				   sort from the shortest span to the
				   longest -->
    <xsl:param name="u"/>     <!-- Units of the averages -->
    <xsl:param name="mt"/>    <!-- Measurement type associated with the
				   averages -->
    <xsl:if test="$avgs">
      <xsl:variable name="avg">
	<xsl:choose>
	  <xsl:when test="substring-before($avgs, ' ')">
	    <xsl:value-of select="substring-before($avgs, ' ')"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of select="$avgs"/>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:variable>
      <td align="right" bgcolor="{$td_bgcolor}">
	<xsl:call-template name="format">
	  <xsl:with-param name="v"  select="$avg"/>
	  <xsl:with-param name="mt" select="$mt"/>
	  <xsl:with-param name="u"  select="$u"/>
	  <xsl:with-param name="du" select="1"/>
	</xsl:call-template>
      </td>
      <xsl:if test="substring-after($avgs, ' ')">
	<xsl:call-template name="averages">
	  <xsl:with-param name="avgs" select="substring-after($avgs, ' ')"/>
	  <xsl:with-param name="u"    select="$u"/>
	  <xsl:with-param name="mt"   select="$mt"/>
	</xsl:call-template>
      </xsl:if>
    </xsl:if>
  </xsl:template>


  <!-- Display daily lows and highs -->

  <xsl:template name="extrema">
    <xsl:param name="low-high"/>  <!-- Low and high values, SP delimited -->
    <xsl:param name="u"/>         <!-- Units of the highs and lows -->
    <xsl:param name="mt"/>        <!-- Measurement type associated with the
				       high and low values -->
    <xsl:param name="times"/>     <!-- Times of the low and high, respectively.
				       Format should be "HH:MM HH:MM" -->
    <xsl:variable name="low-high2" select="normalize-space($low-high)"/>
    <xsl:variable name="low"  select="substring-before($low-high2, ' ')"/>
    <xsl:variable name="high" select="substring-after($low-high2, ' ')"/>
    <td align="center" bgcolor="{$td_bgcolor}">
      <xsl:choose>
	<xsl:when test="$low-high2">
	  <xsl:call-template name="format">
	    <xsl:with-param name="v"  select="$low"/>
	    <xsl:with-param name="mt" select="$mt"/>
	    <xsl:with-param name="u"  select="$u"/>
	  </xsl:call-template>
	  <xsl:text> / </xsl:text>
	  <xsl:call-template name="format">
	    <xsl:with-param name="v"  select="$high"/>
	    <xsl:with-param name="mt" select="$mt"/>
	    <xsl:with-param name="du" select="1"/>
	    <xsl:with-param name="u"  select="$u"/>
	  </xsl:call-template>
	  <xsl:variable name="times2" select="normalize-space($times)"/>
	  <xsl:if test="$times2">
	    <xsl:value-of select="concat(' (',substring-before($times2, ' '),
				  ' / ', substring-after($times2, ' '), ')')"/>
	  </xsl:if>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:text>&#160;</xsl:text>
	</xsl:otherwise>
      </xsl:choose>
    </td>
  </xsl:template>


  <!-- Display a measurement value and it's associated running averages
       and daily highs and lows.  That is, display a <value> element. -->

  <xsl:template match="value">
    <xsl:if test="not(@type = $dewp)">
    <tr>
      <td align="right" bgcolor="{$td_bgcolor}">
	<xsl:choose>
	  <xsl:when test="(@type = $pres) or (@type = $prsl) or
   			  (@type = $prsl12) or (@type = $prsl0)">
	    <xsl:if test="/wstation/station/altitude/@v and not(/wstation/station/altitude/@v = 0)">
	      <xsl:if test="@type = $pres">
		&#160;station
	      </xsl:if>
	      <xsl:if test="(@type = $prsl) or (@type = $prsl12)">
		&#160;sea level (12hr)
	      </xsl:if>
	      <xsl:if test="@type = $prsl0">
		&#160;sea level
	      </xsl:if>
	      <xsl:text>&#160;&#160;</xsl:text>
	    </xsl:if>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>&#160;&#160;&#160;&#160;</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:call-template name="trend">
	  <xsl:with-param name="v"    select="@v"/>
	  <xsl:with-param name="avgs" select="normalize-space(./averages/@v)"/>
	</xsl:call-template>
	<xsl:call-template name="format">
	  <xsl:with-param name="v"  select="@v"/>
	  <xsl:with-param name="mt" select="@type"/>
	  <xsl:with-param name="u"  select="@units"/>
	  <xsl:with-param name="du" select="2"/>
	</xsl:call-template>
      </td>
      <xsl:call-template name="averages">
	<xsl:with-param name="mt"    select="@type"/>
	<xsl:with-param name="avgs"  select="normalize-space(./averages/@v)"/>
	<xsl:with-param name="u"
			select="concat(./averages/@units,substring(@units,1,(number(not(boolean(./averages/@units)))*string-length(@units))))"/>
      </xsl:call-template>
      <xsl:call-template name="extrema">
	<xsl:with-param name="mt"       select="@type"/>
	<xsl:with-param name="u"
			select="concat(./extrema/@units,substring(@units,1,(number(not(boolean(./extrema/@units)))*string-length(@units))))"/>
	<xsl:with-param name="low-high" select="./extrema/@v"/>
	<xsl:with-param name="times"    select="./extrema/@time"/>
      </xsl:call-template>
      <xsl:call-template name="extrema">
	<xsl:with-param name="mt"       select="@type"/>
	<xsl:with-param name="u"
			select="concat(./yesterday/extrema/@units,substring(@units,1,(number(not(boolean(./yesterday/extrema/@units)))*string-length(@units))))"/>
	<xsl:with-param name="low-high" select="./yesterday/extrema/@v"/>
	<xsl:with-param name="times"    select="./yesterday/extrema/@time"/>
      </xsl:call-template>
    </tr>
    </xsl:if>
    <xsl:if test="@type = $dewp">
    <tr>
      <td colspan="5" align="left" bgcolor="{$td_bgcolor}">
        <xsl:text>&#160;&#160;&#160;</xsl:text>
	<xsl:value-of select="$dewp-display"/><xsl:text> </xsl:text>
        <xsl:call-template name="format">
	  <xsl:with-param name="v" select="@v"/>
	  <xsl:with-param name="mt" select="@type"/>
	  <xsl:with-param name="u"  select="@units"/>
	  <xsl:with-param name="du" select="1"/>
	</xsl:call-template>
      </td>
    </tr>
    </xsl:if>
  </xsl:template>

  <xsl:template name="sensor-table-head-periods">
    <xsl:param name="periods"/>          <!-- SP delimited list of averaging
					      periods -->
    <xsl:param name="u" select="$u-s"/>  <!-- Units -->
    <xsl:if test="$periods">
      <!-- Pick off the first period from the list of periods -->
      <xsl:variable name="period">
	<xsl:choose>
	  <xsl:when test="substring-before($periods, ' ')">
	    <xsl:value-of select="substring-before($periods, ' ')"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of select="$periods"/>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:variable>
      <xsl:variable name="p">
	<xsl:call-template name="to-s">
	  <xsl:with-param name="v" select="$period"/>
	  <xsl:with-param name="u"
			  select="concat(substring($u-s,1,(number(not(boolean($u)))*string-length($u-s))),$u)"/>
	</xsl:call-template>
      </xsl:variable>
      <th bgcolor="{$th2_bgcolor}">
	<font size="-1" face="Arial, Helvetica, sans-serif">
	  <xsl:choose>
	    <xsl:when test="$p &lt; 60">
	      <xsl:value-of select="concat($p,' s')"/>
	    </xsl:when>
	    <xsl:when test="$p &lt; 3600">
	      <xsl:value-of
		 select="concat(round(number($p) div 60),' min')"/>
	    </xsl:when>
	    <xsl:when test="$p &lt; 86400">
	      <xsl:variable name="x" select="round(number($p) div 3600)"/>
	      <xsl:value-of select="concat($x, ' hour')"/>
	      <xsl:if test="not($x = 1)">
		<xsl:text>s</xsl:text>
	      </xsl:if>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:variable name="x" select="round(number($p) div 86400)"/>
	      <xsl:value-of select="concat($x, ' day')"/>
	      <xsl:if test="not($x = 1)">
		<xsl:text>s</xsl:text>
	      </xsl:if>
	    </xsl:otherwise>
	  </xsl:choose>
	</font>
      </th>
      <!-- Make a recursive call with the periods 2,3,... from $periods. -->
      <!-- If we use str:tokenize() we can avoid this recursive call by
	   instead doing a for-each loop on str:tokenize($periods, ' ').
	   However, since the list of periods is expected to be small,
	   and doing the recursion is easy, we'll skip using
	   str:tokenize(). -->
      <xsl:if test="substring-after($periods, ' ')">
	<xsl:call-template name="sensor-table-head-periods">
	  <xsl:with-param name="periods"
			  select="substring-after($periods, ' ')"/>
	  <xsl:with-param name="u" select="$u"/>
	</xsl:call-template>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <!-- Display the table heads for a <sensor> element -->

  <xsl:template name="sensor-table-heads">
    <xsl:param name="id" select="'???'"/>
    <xsl:param name="periods"/>              <!-- SP delimited list of the
						  averaging periods -->
    <xsl:param name="u" select="$u-s"/>      <!-- Units of the periods -->
    <xsl:param name="description"
	       select="'Unknown Location'"/> <!-- Sensor description -->
    <!-- Just because the parameter has a default doesn't safeguard
	 us against being passed an empty string for the units -->
    <tr>
      <th rowspan="2" valign="center" align="left" bgcolor="{$th1_bgcolor}">
	<font size="+1" face="Arial, Helvetica, sans-serif">
	  <xsl:value-of select="concat('&#160;&#160;',description)"/>
	</font>
 	<xsl:if test="contains($calibrating,$id)"><font size="-1" face="Arial, Helvetica, sans-serif"><xsl:text> (calibrating)</xsl:text></font></xsl:if>
      </th>
      <xsl:if test="$periods">
	<th colspan="{1 + string-length(normalize-space($periods)) - string-length(translate(normalize-space($periods),' ',''))}" align="center" bgcolor="{$th1_bgcolor}">
	  <font face="Arial, Helvetica, sans-serif">Running Averages</font>
	</th>
      </xsl:if>
      <th colspan="2" align="center" bgcolor="{$th1_bgcolor}">
	<font face="Arial, Helvetica, sans-serif">Daily Lows &amp; Highs</font>
      </th>
    </tr>
    <tr>
      <xsl:call-template name="sensor-table-head-periods">
	<xsl:with-param name="periods" select="normalize-space($periods)"/>
	<xsl:with-param name="u"       select="$u"/>
      </xsl:call-template>
      <th bgcolor="{$th2_bgcolor}">
	<font size="-1" face="Arial, Helvetica, sans-serif">Today</font>
      </th>
      <th bgcolor="{$th2_bgcolor}">
	<font size="-1" face="Arial, Helvetica, sans-serif">Yesterday</font>
      </th>
    </tr>
  </xsl:template>


  <!-- Display a <sensor> element -->

  <xsl:template match="sensor">
    <xsl:call-template name="sensor-table-heads">
      <xsl:with-param name="id"      select="@id"/>
      <xsl:with-param name="title"   select="description"/>
      <xsl:with-param name="periods" select="normalize-space(averages/@p)"/>
      <xsl:with-param name="u"       select="averages/@p-units"/>
    </xsl:call-template>
    <xsl:apply-templates select="value[not(@type='prsl0')]"/>
    <xsl:if test="not(position() = last())">
      <tr>
	<td border="0" bgcolor="{$bg_color}" colspan="7">&#160;</td>
      </tr>
    </xsl:if>
  </xsl:template>

  <xsl:template match="station">
    <xsl:if test="altitude/@v or latitude/@v or longitude/@v">
      <p>
	<table border="0">
	  <xsl:if test="longitude/@v">
	    <tr>
	      <td align="right">Longitude: </td>
	      <td align="left"><xsl:value-of select="longitude/@v"/></td>
	    </tr>
	  </xsl:if>
	  <xsl:if test="latitude/@v">
	    <tr>
	      <td align="right">Latitude: </td>
	      <td align="left"><xsl:value-of select="latitude/@v"/></td>
	    </tr>
	  </xsl:if>
	  <xsl:if test="altitude/@v">
	    <tr>
	      <td align="right">Altitude: </td>
	      <td align="left">
		<xsl:call-template name="format">
		  <xsl:with-param name="v" select="altitude/@v"/>
		  <xsl:with-param name="mt" select="$leng"/>
		  <xsl:with-param name="u" select="altitude/@units"/>
		  <xsl:with-param name="du" select="1"/>
		</xsl:call-template>
	      </td>
	    </tr>
	  </xsl:if>
	</table>
      </p>
    </xsl:if>
  </xsl:template>

  <!-- Here's where we get started -->

  <xsl:template match="/">
    <xsl:choose>
      <xsl:otherwise>
	<xsl:variable name="location">
	  <xsl:choose>
	    <xsl:when test="/wstation/@name">
	      <xsl:value-of select="/wstation/@name"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="'HA7Netd'"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:variable>
	<html>
	  <head>
	    <xsl:variable name="refresh-period">
	      <xsl:choose>
		<xsl:when test="/wstation/@period">
		  <xsl:value-of select="/wstation/@period"/>
		</xsl:when>
		<xsl:otherwise>
		  120
		</xsl:otherwise>
	      </xsl:choose>
            </xsl:variable>
	    <META http-equiv="refresh" content="{$refresh-period};"/>
	    <META name="description" content="Weather data for Mt. Baldy Village, California"/>
	    <title>
	      <xsl:value-of select="concat($location,' Environmental Data')"/>
	    </title>
	  </head>
	  <body text="{$txt_color}" bgcolor="{$bg_color}">
	    <p>
	      <xsl:if test="/wstation/@name">
		<xsl:value-of select="/wstation/@name"/>
		<br/>
	      </xsl:if>
	      <xsl:if test="/wstation/@date">
		<xsl:value-of select="/wstation/@date"/>
	      </xsl:if>
	      <xsl:text> </xsl:text>
	      <xsl:if test="/wstation/@time">
		<xsl:value-of select="/wstation/@time"/>
	      </xsl:if>
	    </p>
	    <p>
	      Current weather conditions for Mt. Baldy Village, California
              (<a target="_blank"
		href="http://maps.google.com/maps?q=34.2361,-117.6590&amp;btnG=Search&amp;sc=1&amp;rl=1&amp;spn=0.3,0.3">road map</a>;
               <a target="_blank"
		href="http://www.topozone.com/map.asp?lat=34.2361&amp;lon=-117.6590&amp;size=l&amp;u=5">topo map</a>).
		<br/>
		Please click <a href="about.html">here</a> for
	      further information about this page.
	    </p>
	    <xsl:apply-templates select="/wstation/station"/>
	    <p>
	      <table cellpadding="3" cellspacing="1" border="0"
		     bgcolor="{$txt_color}">
		<xsl:apply-templates select="/wstation/sensor">
	          <xsl:sort select="description"/>
	        </xsl:apply-templates>
	      </table>
	    </p>
	    <p>
	      <a href="about.html">About this page</a>
	    </p>
	  </body>
	</html>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

</xsl:stylesheet>
