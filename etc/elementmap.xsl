<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform' xmlns:click='http://www.lcdf.org/click/xml/'>


<xsl:template match='/'>
<html>
<title>Click elementmap</title>
<style type='text/css'>
TD { padding: 2px 10px 2px 2px; }
TR { vertical-align: top; }
TR.l1 { background-color: #ffffe9; }
TR.l2 { background-color: #efefe9; }
TR.lh { background-color: #d9d4f7; }
SPAN.element { font-weight: bold; }
</style>
<body>

<h1>Click elementmap</h1>

<xsl:apply-templates />

</body>
</html>
</xsl:template>



<xsl:template match='click:elementmap' >

<xsl:if test='@drivers'>
  <p><i>Drivers: </i>
  <xsl:value-of select='@drivers' /></p>
</xsl:if>

<xsl:if test='@provides'>
  <p><i>Provides: </i>
  <xsl:value-of select='@provides' /></p>
</xsl:if>

<xsl:if test='click:entry[@name]'>
<h2>Elements</h2>

<table cellspacing='0' cellpadding='2' border='0'>

<tr class='lh'>
  <th>Element</th>
  <th>Properties</th>
</tr>

<xsl:for-each select='click:entry[@name]'>
<tr>
  <xsl:attribute name='class'>
    <xsl:if test='position() mod 2 = 0'>l2</xsl:if>
    <xsl:if test='position() mod 2 = 1'>l1</xsl:if>
  </xsl:attribute>

<td><span class='element'>
  <xsl:choose>
    <xsl:when test='//@dochref'>
      <a><xsl:attribute name='href'><xsl:value-of select='substring-before(//@dochref, "%s")' /><xsl:value-of select='@name' /><xsl:value-of select='substring-after(//@dochref, "%s")' /></xsl:attribute>
	<xsl:value-of select='@name' /></a>
    </xsl:when>
    <xsl:otherwise><xsl:value-of select='@name' /></xsl:otherwise>
  </xsl:choose>
</span></td>

<td>

  <xsl:if test='@processing or @flowcode'>
    <xsl:if test='@processing'>
      <i>Processing: </i>
      "<tt><xsl:value-of select='@processing' /></tt>"
    </xsl:if>
    <xsl:if test='@processing and @flowcode'>, </xsl:if>
    <xsl:if test='@flowcode'>
      <i>Flowcode: </i>
      "<tt><xsl:value-of select='@flowcode' /></tt>"
    </xsl:if>
    <br />
  </xsl:if>

  <xsl:if test='@requires or @provides'>
    <xsl:if test='@requires'>
      <i>Requires: </i>
      <xsl:value-of select='@requires' />
    </xsl:if>
    <xsl:if test='@requires and @provides'>, </xsl:if>
    <xsl:if test='@provides'>
      <i>Provides: </i>
      <xsl:value-of select='@provides' />
    </xsl:if>
    <br />
  </xsl:if>

  <xsl:if test='@headerfile or @sourcefile'>
    <i>Source: </i>
    <xsl:if test='@headerfile'>
      <a><xsl:attribute name='href'><xsl:value-of select='//@src' />/<xsl:if test='starts-with(@headerfile, "&lt;")'>include/<xsl:value-of select='substring(@headerfile, 2, string-length(@headerfile) - 2)' /></xsl:if><xsl:if test='not(starts-with(@headerfile, "&lt;"))'><xsl:value-of select='@headerfile' /></xsl:if></xsl:attribute>
	<xsl:value-of select='@headerfile' />
      </a>
    </xsl:if>
    <xsl:if test='@headerfile and @sourcefile'>, </xsl:if>
    <xsl:if test='@sourcefile'>
      <a><xsl:attribute name='href'><xsl:value-of select='//@src' />/<xsl:value-of select='@sourcefile' /></xsl:attribute>
        <xsl:value-of select='@sourcefile' />
      </a>
    </xsl:if>
    <br />
  </xsl:if>
</td>

</tr>
</xsl:for-each>

</table>
</xsl:if>

<xsl:if test='click:entry[not(@name)]'>
<h2>Provisions</h2>

<table cellspacing='0' cellpadding='2' border='0'>

<tr class='lh'>
  <th>Provides</th>
  <th>Properties</th>
</tr>

<xsl:for-each select='click:entry[not(@name)]'>

<tr>
  <xsl:attribute name='class'>
    <xsl:if test='position() mod 2 = 0'>l2</xsl:if>
    <xsl:if test='position() mod 2 = 1'>l1</xsl:if>
  </xsl:attribute>

<td><strong>
  <xsl:value-of select='@provides' />
</strong></td>

<td>
  <xsl:if test='@processing or @flowcode'>
    <xsl:if test='@processing'>
      <i>Processing: </i>
      "<tt><xsl:value-of select='@processing' /></tt>"
    </xsl:if>
    <xsl:if test='@processing and @flowcode'>, </xsl:if>
    <xsl:if test='@flowcode'>
      <i>Flowcode: </i>
      "<tt><xsl:value-of select='@flowcode' /></tt>"
    </xsl:if>
    <br />
  </xsl:if>

  <xsl:if test='@requires'>
    <i>Requires: </i>
    <xsl:value-of select='@requires' />
    <br />
  </xsl:if>

  <xsl:if test='@headerfile or @sourcefile'>
    <i>Source: </i>
    <xsl:if test='@headerfile'>
      <a><xsl:attribute name='href'><xsl:value-of select='//@src' />/<xsl:if test='starts-with(@headerfile, "&lt;")'>include/<xsl:value-of select='substring(@headerfile, 2, string-length(@headerfile) - 2)' /></xsl:if><xsl:if test='not(starts-with(@headerfile, "&lt;"))'><xsl:value-of select='@headerfile' /></xsl:if></xsl:attribute>
	<xsl:value-of select='@headerfile' />
      </a>
    </xsl:if>
    <xsl:if test='@headerfile and @sourcefile'>, </xsl:if>
    <xsl:if test='@sourcefile'>
      <a><xsl:attribute name='href'><xsl:value-of select='//@src' />/<xsl:value-of select='@sourcefile' /></xsl:attribute>
        <xsl:value-of select='@sourcefile' />
      </a>
    </xsl:if>
    <br />
  </xsl:if>
</td>

</tr>
</xsl:for-each>

</table>
</xsl:if>

</xsl:template>


</xsl:stylesheet>
