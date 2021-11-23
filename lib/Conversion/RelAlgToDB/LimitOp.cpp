#include "mlir/Conversion/RelAlgToDB/ProducerConsumerNode.h"
#include "mlir/Dialect/DB/IR/DBOps.h"
#include "mlir/Dialect/RelAlg/IR/RelAlgOps.h"
#include "mlir/Dialect/util/UtilOps.h"

class LimitLowering : public mlir::relalg::ProducerConsumerNode {
   mlir::relalg::LimitOp limitOp;
   size_t counterId;
   mlir::Value vector;
   mlir::Value finishedFlag;

   public:
   LimitLowering(mlir::relalg::LimitOp limitOp) : mlir::relalg::ProducerConsumerNode(limitOp.rel()), limitOp(limitOp) {
   }
   virtual void setInfo(mlir::relalg::ProducerConsumerNode* consumer, mlir::relalg::Attributes requiredAttributes) override {
      this->consumer = consumer;
      this->requiredAttributes = requiredAttributes;
      this->requiredAttributes.insert(limitOp.getUsedAttributes());
      propagateInfo();
   }
   virtual void addRequiredBuilders(std::vector<size_t> requiredBuilders) override{
      this->requiredBuilders.insert(this->requiredBuilders.end(), requiredBuilders.begin(), requiredBuilders.end());
      this->children[0]->addRequiredBuilders(requiredBuilders);
   }
   virtual mlir::relalg::Attributes getAvailableAttributes() override {
      return this->children[0]->getAvailableAttributes();
   }
   virtual void consume(mlir::relalg::ProducerConsumerNode* child, mlir::OpBuilder& builder, mlir::relalg::LoweringContext& context) override {
      std::vector<mlir::Type> types;
      std::vector<mlir::Value> values;
      for (const auto* attr : requiredAttributes) {
         types.push_back(attr->type);
         values.push_back(context.getValueForAttribute(attr));
      }
      mlir::Value counter = context.builders[counterId];
      consumer->consume(this, builder, context);
      auto one = builder.create<mlir::db::ConstantOp>(builder.getUnknownLoc(), counter.getType(), builder.getI64IntegerAttr(1));
      mlir::Value addedCounter = builder.create<mlir::db::AddOp>(builder.getUnknownLoc(), counter.getType(), counter, one);
      mlir::Value upper=builder.create<mlir::db::ConstantOp>(limitOp.getLoc(),counter.getType(),builder.getI64IntegerAttr(limitOp.rows()));
      mlir::Value finished=builder.create<mlir::db::CmpOp>(limitOp.getLoc(),mlir::db::DBCmpPredicate::gte,addedCounter,upper);
      builder.create<mlir::db::SetFlag>(limitOp->getLoc(), finishedFlag,finished);
      context.builders[counterId] = addedCounter;
   }

   virtual void produce(mlir::relalg::LoweringContext& context, mlir::OpBuilder& builder) override {
      auto scope = context.createScope();
      std::unordered_map<const mlir::relalg::RelationalAttribute*, size_t> attributePos;
      std::vector<mlir::Type> types;
      size_t i = 0;
      for (const auto* attr : requiredAttributes) {
         types.push_back(attr->type);
         attributePos[attr] = i++;
      }
      mlir::Value counter = builder.create<mlir::db::ConstantOp>(limitOp.getLoc(), mlir::db::IntType::get(builder.getContext(), false, 64),builder.getI64IntegerAttr(0));
      finishedFlag = builder.create<mlir::db::CreateFlag>(limitOp->getLoc(), mlir::db::FlagType::get(builder.getContext()));
      children[0]->setFlag(finishedFlag);
      counterId = context.getBuilderId();
      context.builders[counterId] = counter;

      children[0]->addRequiredBuilders({counterId});
      children[0]->produce(context, builder);
   }

   virtual ~LimitLowering() {}
};

bool mlir::relalg::ProducerConsumerNodeRegistry::registeredLimitOp = mlir::relalg::ProducerConsumerNodeRegistry::registerNode([](mlir::relalg::LimitOp limitOp) {
  return std::make_unique<LimitLowering>(limitOp);
});