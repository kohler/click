<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'  xmlns:click='http://www.lcdf.org/click/xml/'>

<xsl:template match='/'>
<html>
<title>Click elementmap</title>
<style type='text/css'>
TD {
  padding: 2px 10px 2px 2px;
}
TR.l1 {
  background-color: #ffffc9;
}
TR.l2 {
  background-color: #f9f9ff;
}
TR.lh {
  background-color: #d9d4f7;
}
</style>
<body>
<h1>Click elementmap</h1>
<xsl:apply-templates />
</body>
</html>
</xsl:template>

<xsl:template match='click:elementmap'>
<table cellspacing='0' cellpadding='2' border='0'>
<tr class='lh'>
  <th>Element name</th>
  <th>C++ class</th>
  <th>Requires</th>
  <th>Provides</th>
</tr>
  <xsl:for-each select='click:entry'>
    <tr>
    <xsl:attribute name='class'>
      <xsl:if test='position() mod 2 = 0'>l2</xsl:if>
      <xsl:if test='position() mod 2 = 1'>l1</xsl:if>
    </xsl:attribute>
    <td><xsl:value-of select='@name' /></td>
    <td><xsl:value-of select='@cxxclass' /></td>
    <td><xsl:value-of select='@requires' /></td>
    <td><xsl:value-of select='@provides' /></td>
    </tr>
  </xsl:for-each>
</table>
</xsl:template>

</xsl:stylesheet>
