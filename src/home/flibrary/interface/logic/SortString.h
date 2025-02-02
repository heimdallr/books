#pragma once

struct QStringWrapper
{
	const QString & data;
	bool operator<(const QStringWrapper & rhs) const
	{
		if (data.isEmpty() || rhs.data.isEmpty())
			return !rhs.data.isEmpty();

		const auto lCategory = Category(data[0]), rCategory = Category(rhs.data[0]);
		return lCategory != rCategory ? lCategory < rCategory : QString::compare(data, rhs.data, Qt::CaseInsensitive) < 0;
	}

private:
	[[nodiscard]] int Category(const QChar c) const noexcept
	{
		assert(c.category() < static_cast<int>(std::size(m_categories)));
		if (const auto result = m_categories[c.category()]; result != 0)
			return result;

		return c.row() == 4 ? 0 : 1;
	}

private:
	static constexpr int m_categories[]
	{
0,	//		Mark_NonSpacing,          //   Mn
0,	//		Mark_SpacingCombining,    //   Mc
0,	//		Mark_Enclosing,           //   Me
//
2,	//		Number_DecimalDigit,      //   Nd
0,	//		Number_Letter,            //   Nl
0,	//		Number_Other,             //   No
//
0,	//		Separator_Space,          //   Zs
0,	//		Separator_Line,           //   Zl
0,	//		Separator_Paragraph,      //   Zp
//
8,	//		Other_Control,            //   Cc
8,	//		Other_Format,             //   Cf
8,	//		Other_Surrogate,          //   Cs
8,	//		Other_PrivateUse,         //   Co
8,	//		Other_NotAssigned,        //   Cn
//
0,	//		Letter_Uppercase,         //   Lu
0,	//		Letter_Lowercase,         //   Ll
0,	//		Letter_Titlecase,         //   Lt
0,	//		Letter_Modifier,          //   Lm
0,	//		Letter_Other,             //   Lo
//
6,	//		Punctuation_Connector,    //   Pc
6,	//		Punctuation_Dash,         //   Pd
6,	//		Punctuation_Open,         //   Ps
6,	//		Punctuation_Close,        //   Pe
6,	//		Punctuation_InitialQuote, //   Pi
6,	//		Punctuation_FinalQuote,   //   Pf
6,	//		Punctuation_Other,        //   Po
//
4,	//		Symbol_Math,              //   Sm
4,	//		Symbol_Currency,          //   Sc
4,	//		Symbol_Modifier,          //   Sk
4,	//		Symbol_Other              //   So
	};
};
