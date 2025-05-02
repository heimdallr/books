#include "AuthorsModel.h"

#include <QSize>

#include "fnd/algorithm.h"

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IAuthorAnnotationController.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

struct AuthorsModel::Impl : private IAuthorAnnotationController::IObserver
{
	QAbstractItemModel* self;
	PropagateConstPtr<IAuthorAnnotationController, std::shared_ptr> authorAnnotationController;
	bool isReady { authorAnnotationController->IsReady() };

	Impl(QAbstractItemModel* self, std::shared_ptr<IAuthorAnnotationController> authorAnnotationController)
		: self { self }
		, authorAnnotationController { std::move(authorAnnotationController) }
	{
		this->authorAnnotationController->RegisterObserver(this);
	}

	~Impl() override
	{
		authorAnnotationController->UnregisterObserver(this);
	}

private: // IAuthorAnnotationController::IObserver
	void OnReadyChanged() override
	{
		if (Util::Set(isReady, authorAnnotationController->IsReady()))
			self->dataChanged(self->index(0, 0), self->index(self->rowCount() - 1, 0), { Qt::CheckStateRole });
	}

	void OnAuthorChanged(const QString&, const std::vector<QByteArray>&) override
	{
	}

	NON_COPY_MOVABLE(Impl)
};

AuthorsModel::AuthorsModel(const std::shared_ptr<IModelProvider>& modelProvider, std::shared_ptr<IAuthorAnnotationController> authorAnnotationController, QObject* parent)
	: ListModel(modelProvider, parent)
	, m_impl(this, std::move(authorAnnotationController))
{
	PLOGV << "AuthorsModel created";
}

AuthorsModel::~AuthorsModel()
{
	PLOGV << "AuthorsModel destroyed";
}

int AuthorsModel::columnCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : 2;
}

QVariant AuthorsModel::data(const QModelIndex& index, const int role) const
{
	if (index.column() != 1)
		return ListModel::data(index, role);

	const auto author = static_cast<const QAbstractItemModel*>(this)->index(index.row(), 0, index.parent()).data().toString();
	switch (role)
	{
		case Qt::DisplayRole:
			return m_impl->authorAnnotationController->CheckAuthor(author) ? Constant::INFO : QVariant {};

		case Qt::ToolTipRole:
			return m_impl->authorAnnotationController->GetInfo(author);

		default:
			break;
	}

	return {};
}
