<?xml version="1.0" encoding="UTF-8"?>

<!-- Copyright (c) 2005, Daniel C. Newman <dan.newman@mtbaldy.us>
     All rights reserved.
     See the file COPYRIGHT for further information. -->

<!-- *** DO NOT EDIT THIS FILE ***
     *** This file was automatically generated by make_includes.c
     *** from the source file make_includes.conf. -->

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <!-- String used to indicate a missing data value -->
  <xsl:variable name="missing-value" select="'*'"/>

  <!-- Names of the various measurement types -->
  <xsl:variable name="dewp"    select="'dewp'"/>  <!-- Dew point -->
  <xsl:variable name="leng"    select="'leng'"/>  <!-- Length -->
  <xsl:variable name="pres"    select="'pres'"/>  <!-- Atmospheric pressure, station altitude -->
  <xsl:variable name="prsl"    select="'prsl'"/>  <!-- Atmospheric pressure, mean sea level (12 hour averaging) -->
  <xsl:variable name="prsl0"   select="'prsl0'"/> <!-- Atmospheric pressure, mean sea level (no averaging) -->
  <xsl:variable name="rain"    select="'rain'"/>  <!-- Precipitation -->
  <xsl:variable name="rh"      select="'rh'"/>    <!-- Relative humidity -->
  <xsl:variable name="temp"    select="'temp'"/>  <!-- Temperature -->
  <xsl:variable name="wdir"    select="'wdir'"/>  <!-- Wind direction -->
  <xsl:variable name="wvel"    select="'wvel'"/>  <!-- Wind velocity -->

  <!-- Time -->
  <xsl:variable name="u-s"     select="'s'"/>     <!-- seconds -->
  <xsl:variable name="u-min"   select="'min'"/>   <!-- minutes -->
  <xsl:variable name="u-hr"    select="'hr'"/>    <!-- hours -->
  <xsl:variable name="u-d"     select="'d'"/>     <!-- days -->

  <!-- Distance -->
  <xsl:variable name="u-m"     select="'m'"/>     <!-- meters -->
  <xsl:variable name="u-mm"    select="'mm'"/>    <!-- millimeters -->
  <xsl:variable name="u-cm"    select="'cm'"/>    <!-- centimeters -->
  <xsl:variable name="u-km"    select="'km'"/>    <!-- kilometers -->
  <xsl:variable name="u-ft"    select="'ft'"/>    <!-- feet -->
  <xsl:variable name="u-mi"    select="'mi'"/>    <!-- miles -->
  <xsl:variable name="u-in"    select="'in'"/>    <!-- inches -->

  <!-- Temperature -->
  <xsl:variable name="u-C"     select="'C'"/>     <!-- Celsius -->
  <xsl:variable name="u-K"     select="'K'"/>     <!-- Kelvin -->
  <xsl:variable name="u-F"     select="'F'"/>     <!-- Fahrenheit -->

  <!-- Velocity -->
  <xsl:variable name="u-kph"   select="'kph'"/>   <!-- kilometers per hour -->
  <xsl:variable name="u-mph"   select="'mph'"/>   <!-- miles per hour -->

  <!-- Pressure -->
  <xsl:variable name="u-atm"   select="'atm'"/>   <!-- Atmospheres -->
  <xsl:variable name="u-Pa"    select="'Pa'"/>    <!-- Pascals -->
  <xsl:variable name="u-hPa"   select="'hPa'"/>   <!-- hecto Pascals -->
  <xsl:variable name="u-kPa"   select="'kPa'"/>   <!-- kilo Pascals -->
  <xsl:variable name="u-mbar"  select="'mbar'"/>  <!-- millibar -->
  <xsl:variable name="u-mb"    select="'mb'"/>    <!-- millibar -->
  <xsl:variable name="u-mmHg"  select="'mmHg'"/>  <!-- millimeters Mercury -->
  <xsl:variable name="u-Torr"  select="'Torr'"/>  <!-- Torricellis -->
  <xsl:variable name="u-inHg"  select="'inHg'"/>  <!-- inches Mercury -->
  <xsl:variable name="u-at"    select="'at'"/>    <!-- technical atmospheres -->

  <!-- Miscellaneous -->
  <xsl:variable name="u-rh"    select="'%'"/>     <!-- Relative Humidity -->
  <xsl:variable name="u-wf"    select="'wf'"/>    <!-- Wind is blowing from -->
  <xsl:variable name="u-wt"    select="'wt'"/>    <!-- Wind is blowing towards -->

</xsl:stylesheet>